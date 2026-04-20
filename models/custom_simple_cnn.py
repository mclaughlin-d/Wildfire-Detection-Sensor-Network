import torch
import torch.nn as nn
import torch.optim as optim
from torchvision import datasets, transforms
import torch.nn.functional as F
from torchmetrics import ConfusionMatrix


if torch.cuda.is_available():       
    device = torch.device("cuda")
    print(f'There are {torch.cuda.device_count()} GPU(s) available.')
    print('Device name:', torch.cuda.get_device_name(0))

else:
    print('No GPU available, using the CPU instead.')
    device = torch.device("cpu")

transform = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(), # Scales to [0, 1]
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

train_dataset = datasets.ImageFolder(root='FOREST_FIRE_SMOKE_AND_NON_FIRE_DATASET/train', transform=transform)
train_dataloader = torch.utils.data.DataLoader(train_dataset, batch_size=32, shuffle=True, num_workers=4)
test_dataset = datasets.ImageFolder(root='FOREST_FIRE_SMOKE_AND_NON_FIRE_DATASET/test', transform=transform)
test_dataloader = torch.utils.data.DataLoader(test_dataset, batch_size=32, shuffle=True)

dataset_sizes = {'train': len(train_dataset), 'test': len(test_dataset)}
num_classes = 3

"""
Simple CNN architecture, used for image classification
"""
class SimpleCNN(nn.Module):
    def __init__(self):
        super(SimpleCNN, self).__init__()
        self.conv1 = nn.Conv2d(3, 32, 3, padding=1)
        self.pool = nn.MaxPool2d(3, 3)
        self.conv2 = nn.Conv2d(32, 64, 3, padding=1)
        self.conv3 = nn.Conv2d(64, 128, 3, padding=1)
        self.pool2 = nn.AdaptiveAvgPool2d((1, 1))
        self.drop = nn.Dropout(0.1)
        self.fc2 = nn.Linear(128, num_classes)

    def forward(self, x):
        # Apply 3 convolutions and then an average pooling layer
        x = self.pool(F.relu(self.conv1(x)))
        x = self.pool(F.relu(self.conv2(x)))
        x = self.pool(F.relu(self.conv3(x)))
        x = self.pool2(x)
        # convert to 1 x 128 for final linear layer (128 -> 3)
        x = x.view(-1, 128)
        x = self.drop(x)
        return self.fc2(F.relu(x))
    

model = SimpleCNN()

print(model)
model.to(device)

# weighted loss function weights fire class more heavily, as fire mispredictions are worse
weights = torch.tensor([1.0, 2.0, 1.0]).to(device)
criterion = nn.CrossEntropyLoss(weight=weights)
optimizer = optim.Adam(model.parameters(), lr=0.001)
model.train()

train_len = len(train_dataloader)
for epoch in range(15):
    print(f'Epoch {epoch + 1}')

    sample = 0
    running_loss = 0.0
    running_correct = 0.0

    for inputs, labels in train_dataloader:
        inputs = inputs.to(device)
        labels = labels.to(device).long()

        optimizer.zero_grad()
        outputs = model(inputs)
        _, preds = torch.max(outputs, 1)
        loss = criterion(outputs, labels)
        loss.backward()
        optimizer.step()

        running_loss += loss.item() * inputs.size(0)
        running_correct += torch.sum(preds == labels.data).item()
        sample += 1

    epoch_loss = running_loss / dataset_sizes['train']
    epoch_acc = running_correct / dataset_sizes['train']
    print(f'\tTrain Loss: {epoch_loss:.4f} Acc: {epoch_acc:.4f}')

torch.save(model.state_dict(), './custom_cnn.pth')


model.eval()

test_correct = 0
confmat = ConfusionMatrix(task="multiclass", num_classes=3).to(device)


missed_fire_preds = 0

with torch.no_grad():
    for inputs, labels in test_dataloader:
        inputs = inputs.to(device)
        labels = labels.to(device)
        outputs = model(inputs)
        _, preds = torch.max(outputs, 1)
        test_correct += torch.sum(preds == labels).item()
        confmat.update(preds, labels)

print(f'Test Acc: {test_correct / dataset_sizes["test"]:.4f}')

matrix = confmat.compute()
print(matrix)
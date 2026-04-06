import torch
import torch.nn as nn
import torch.optim as optim
from torchvision import datasets, transforms
from torchvision.models import resnet50, ResNet50_Weights
import argparse
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
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

def main():
    parser = argparse.ArgumentParser(description="A script to fine-tune the ResNet50 model on a given dataset")
    parser.add_argument("dataset_path", help="Path to the root directory of the dataset. Assumes existence of test/ and train/ sub-directories")
    parser.add_argument("--epochs", help="Number of epochs to fine-tune for", default=3)
    parser.add_argument("--outfile", help="Path to save the final state dict to", default="fine_tuned.pth")
    parser.add_argument("--batch_size", help="Batch size for fine-tuning", default=32)

    args = parser.parse_args()
    base_path = args.dataset_path
    epochs = int(args.epochs)
    outfile = args.outfile
    batch_size = args.batch_size

    train_dataset = datasets.ImageFolder(root=f'{base_path}/train', transform=transform)
    train_dataloader = torch.utils.data.DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    test_dataset = datasets.ImageFolder(root=f'{base_path}/test', transform=transform)
    test_dataloader = torch.utils.data.DataLoader(test_dataset, batch_size=batch_size, shuffle=True)

    dataset_sizes = {'train': len(train_dataset), 'test': len(test_dataset)}
    num_classes = len(train_dataset.classes)

    print(f"Fine-tuning on {dataset_sizes['train']} train samples")
    print(f"Classes: {train_dataset.classes}")
    
    model = resnet50(weights=ResNet50_Weights.IMAGENET1K_V2)
    model.fc = nn.Linear(model.fc.in_features, num_classes)
    model = model.to(device)

    weights = torch.tensor([1.0, 2.0, 1.0]).to(device)
    criterion = nn.CrossEntropyLoss(weight=weights)
    optimizer = optim.Adam(model.parameters(), lr=0.001)

    model.train()

    for epoch in range(epochs):
        print(f'Epoch {epoch + 1}')
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

        epoch_loss = running_loss / dataset_sizes['train']
        epoch_acc = running_correct / dataset_sizes['train']
        print(f'Train Loss: {epoch_loss:.4f} Acc: {epoch_acc:.4f}')

    model.eval()

    test_correct = 0
    confmat = ConfusionMatrix(task="multiclass", num_classes=3).to(device)
    print(f"Testing on {dataset_sizes['test']} test samples")
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

    torch.save(model.state_dict(), outfile)

if __name__ == "__main__":
    main()
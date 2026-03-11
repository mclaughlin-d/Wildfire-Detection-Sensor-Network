## Model-Related Scripts

### fine_tune_model.py
This script is used to fine-tune a ResNet50 CNN model based on a given dataset path. This model is primarily used for image classification.


#### Requirements
- PyTorch and Torchvision
- System with a usable GPU, ideally
- path to a dataset to train on
- Ideally, a system with a GPU (training took a while on a system _with_ a GeForce RTX 3070 Laptop GPU, so it will likely take much longer on a CPU)

#### Example Usage:
This is the output when trained on the free Kaggle [Forest Fire, Smoke, and Non-Fire dataset](https://www.kaggle.com/datasets/amerzishminha/forest-fire-smoke-and-non-fire-image-dataset). Download the `.zip` file from this link, unzip it, and pass the path into the script in order to fine-tune.   

```

```

### stream_and_classify.py
This script is for using a fine-tuned ResNet50 model to classify an image as an image of a forest fire, and image of a forest with smoke, or a normal forest image. 

#### Requirements:
- Numpy
- PyTorch and Torchvision
- GStreamer installed with python support
- Existing fine-tuned model state dict (included in repo)

#### Example Usage:
Testing:
```

```

Drone streaming (uses the OpenHD default udp socket of `5600`):
```
TODO




























```
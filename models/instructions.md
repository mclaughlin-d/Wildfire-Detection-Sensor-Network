## Model-Related Scripts

### fine_tune_model.py
This script is used to fine-tune a ResNet50 CNN model based on a given dataset path. This model is primarily used for image classification.


#### Requirements
- PyTorch and Torchvision
- System with a usable GPU, ideally
- path to a dataset to train on
- Ideally, a system with a GPU (training took about an hour on a system _with_ a GeForce RTX 3070 Laptop GPU, so it will likely take much longer on a CPU)

#### Example Usage:
This is the output when trained on the free Kaggle [Forest Fire, Smoke, and Non-Fire dataset](https://www.kaggle.com/datasets/amerzishminha/forest-fire-smoke-and-non-fire-image-dataset). Download the `.zip` file from this link, unzip it, and pass the path into the script in order to fine-tune.   

```
$ python3 fine_tune_model.py ./FOREST_FIRE_SMOKE_AND_NON_FIRE_DATASET
There are 1 GPU(s) available.
Device name: NVIDIA GeForce RTX 3070 Laptop GPU
Fine-tuning on 32398 train samples
Classes: ['Smoke', 'fire', 'non fire']
Epoch 1
Train Loss: 0.1546 Acc: 0.9519
Epoch 2
Train Loss: 0.0957 Acc: 0.9706
Epoch 3
Train Loss: 0.0795 Acc: 0.9754
Testing on 10500 test samples
Test Acc: 0.9779

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
$ python3 stream_and_classify.py file --file ./sample_images/nofire.png 
There are 1 GPU(s) available.
Device name: NVIDIA GeForce RTX 3070 Laptop GPU
Initializing GStreamer...
Streaming... Press Ctrl+C to stop.
	Prediction: No Fire
	Prediction: No Fire
	Prediction: No Fire
	Prediction: No Fire
^CStopping...
```

```
$ python3 stream_and_classify.py file --file ./sample_images/fire.png 
There are 1 GPU(s) available.
Device name: NVIDIA GeForce RTX 3070 Laptop GPU
Initializing GStreamer...
Streaming... Press Ctrl+C to stop.
	Prediction: Fire
	Prediction: Fire
	Prediction: Fire
	Prediction: Fire
^CStopping...

```

```
$ python3 stream_and_classify.py file --file ./sample_images/smoke.png 
There are 1 GPU(s) available.
Device name: NVIDIA GeForce RTX 3070 Laptop GPU
Initializing GStreamer...
Streaming... Press Ctrl+C to stop.
	Prediction: Smoke
	Prediction: Smoke
	Prediction: Smoke
^CStopping...
```
  
Drone streaming (uses the OpenHD default udp socket of `5600`):
```
TODO
```
## Models
This folder contains scripts used to train and run Convolutional Neural Networks on test images, videos, and live drone feeds (streamed to udp sockets). 

These scripts were used with [this Kaggle dataset](https://www.kaggle.com/datasets/amerzishminha/forest-fire-smoke-and-non-fire-image-dataset), which will need to be downloaded and unzipped somewhere on your machine.


### Usage Instructions


#### `custom_simple_cnn.py`
This script contains source code for the simple, custom CNN used for image classification. It trains the model and outputs the result to a file called 'custom_cnn.pth'. Currently, there is no command-line customization supported, so usage just looks like:
```
python3 custom_simple_cnn.py
```
**Note**: this script expects that the Kaggle forest fire dataset is located in this directory. This can be easily changed in the script source code if needed.

#### `fine_tune_model.py`
This script allows the user to fine-tune the ResNet50 model on the forest fire dataset (or any other dataset for which the path is passed in as a command-line argument).

The performance in our report was achieved after fine-tuning for 3 epochs.

Example usage:
```
python3 fine_tune_model.py --epochs 2 --outfile test.pth --batch_size 32 ~/fire_cnn/FOREST_FIRE_SMOKE_AND_NON_FIRE_DATASET/
```

#### `stream_and_classify.py`
This script uses the 'fire_classify_trained.pth' model weights with ResNet50 to classify images from an imx780, streamed to the default OpenHD UDP port of 5600. This script can also be used with existing files (see `sample_images/` for some examples). Note that this script defaults to using the `weighted_fine_tune.pth` file.

**Example commands:**  
Streaming from a file (for model validation):
```
python3 stream_and_classify.py file --file sample_images/fire.png 
```
Streaming from a UDP socket:
```
python3 stream_and_classify.py udp 
```

#### `stream_and_classify_with_visual.py`
This script uses the 'fire_classify_trained.pth' model weights with ResNet50 to classify images from an imx780, streamed to the default OpenHD UDP port of 5600. This is essentially a copy of `stream_and_classify.py`, but it also opens a tkinter window and displays the received frame as well.

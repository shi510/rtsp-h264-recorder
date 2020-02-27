# rtsp-h264-recorder
This project is to record raw h264 frames from rtsp camera.  
It creates two files, data and index file.  
We call these as StorageFiles.  
Data file contains raw h264 frames and it's informations.  
Index file contains pointer location and time of each group of picture in data file.  
StorageFile have file name criterion.  
For example, 2020-01-01@15-30-25.data and 2020-01-01@15-30-25.index.  
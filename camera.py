import ctypes
import cv2
import numpy as np


# Load the shared library
camera_lib = ctypes.CDLL('/home/yusuf/Documents/TracknTrace/aggregasi2/linux64/saveImage/Camera.so')  # Adjust the path to your shared library


class ImageInfo(ctypes.Structure):
    _fields_ = [
        ("pData", ctypes.POINTER(ctypes.c_ubyte)),
        ("nFrameLen", ctypes.c_int),
        ("nWidth", ctypes.c_int),
        ("nHeight", ctypes.c_int)
    ]
# Define the return type and argument types of the init_camera function
camera_lib.init_camera.restype = ctypes.c_int
camera_lib.close.restype = ctypes.c_int
camera_lib.get_string.restype = ctypes.c_char_p
camera_lib.get_image.restype = ImageInfo


# Call the init_camera function
result = camera_lib.init_camera()

# Check the result
if result == 1:
    string_ptr = camera_lib.get_string()
    if string_ptr:
        # Convert the pointer to a Python string
        result_string = ctypes.cast(string_ptr, ctypes.c_char_p).value.decode('utf-8')
        print("String from camera:", result_string)
        image_info = camera_lib.get_image()
        if image_info.pData:
            # Convert the image data to a numpy array
            frame_data = np.ctypeslib.as_array(image_info.pData, shape=(image_info.nHeight, image_info.nWidth))
            print(frame_data)

            # Create an OpenCV image from the numpy array
            image = cv2.cvtColor(frame_data, cv2.COLOR_GRAY2BGR)  # Assuming grayscale image
            cv2.imshow("Image from camera", image)
            cv2.waitKey(0)
            cv2.destroyAllWindows()
            camera_lib.close_camera()

        else:
            print("Failed to get image from camera")
            camera_lib.close_camera()

    else:
        print("Failed to get string from camera")
        camera_lib.close_camera()

        # Handle error and free memory if necessary
        # For example, call a function in C to free the memory
        # libcamera.free_string(string_ptr)
else:
    print("Initialization failed")
    camera_lib.close_camera()


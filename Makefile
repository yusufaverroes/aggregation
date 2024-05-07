Demo:SaveImage.cpp 
	 g++  Camera.cpp -o Camera -g  -Wall -I../../Include  -L. -Wl,-Bdynamic -lMvCodeReaderCtrl -lpthread -ldl -Wl,-rpath=./ -I/usr/include/opencv4 `pkg-config --libs opencv4`

clean:
	rm SaveImage -rf




g++ Camera.cpp -o Camera.so -g -Wall -I../../Include -L. -Wl,-Bdynamic -lMvCodeReaderCtrl -lpthread -ldl -Wl,-rpath=./ -shared -fPIC


g++ -std=c++11 -o broadcastTest broadcastTest.cpp -I/home/widatech/Documents/TracknTrace/aggregasi/aggregation-main/websocketcpp `pkg-config --cflags` -pthread


broadcastTest:

g++ -std=c++11 -o broadcastTest broadcastTest.cpp -I/home/widatech/Documents/TracknTrace/aggregasi/aggregation-main/websocketcpp -Wall -IInclude  -L. -Wl,-Bdynamic -lMvCodeReaderCtrl -lpthread -ldl -Wl,-rpath=./ -I/usr/include/opencv4 `pkg-config --cflags --libs opencv4`
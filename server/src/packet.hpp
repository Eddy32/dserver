#ifndef __FRAME_H
#define __FRAME_H

#include <opencv2/opencv.hpp>
#include <ctime>


const int FRAMES_PER_SEND = 10;
const int JSON_BUFF_SIZE = 76800*2*10;

class Packet
{
public:
  int id_user;
  int id_camera;
  std::time_t timestamp;
  std::vector<cv::Mat> frames;

  Packet();
  Packet(int user, int camera, std::time_t ts, std::vector<cv::Mat> dados );
  ~Packet();
};
int packet_to_json(unsigned char* buf, Packet* pacote);
Packet* json_to_packet(void* buf);

#endif
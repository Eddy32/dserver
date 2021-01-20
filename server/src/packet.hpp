#include <opencv2/opencv.hpp>
#include <ctime>


const int FRAMES_PER_SEND = 100;
const int JSON_BUFF_SIZE = 76800*2*FRAMES_PER_SEND;

class Packet
{
public:
  std::string id_user;
  std::string id_camera;
  std::time_t timestamp;
  std::vector<cv::Mat> frames;
  std::vector<std::string> classes;
  
  Packet();
  Packet(std::string user, std::string camera, std::time_t ts, std::vector<cv::Mat> dados, std::vector<std::string> dados_classes );
  ~Packet();
};
int packet_to_json(unsigned char* buf, Packet* pacote);
Packet* json_to_packet(void* buf);

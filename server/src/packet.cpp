#include <opencv2/opencv.hpp>
#include <sstream>	// for stringstream
#include <json-c/json.h>	// for json
#include <cstring> // for memcpy
#include <stdio.h>
#include "base64.h"
#include "packet.hpp"
#include <iostream>

using namespace cv;
using namespace std;


Packet::Packet()
{
};

Packet::Packet(int user, int camera, std::time_t ts, vector<Mat> dados )
{
  id_user = user;
  id_camera = camera;
  timestamp = ts;
  frames = dados;
};

Packet::~Packet()
{
  std::vector<Mat>().swap(frames);
};

int packet_to_json(unsigned char* buf, Packet* pacote)
{
  stringstream ss;
  
  vector<int> param(2);
  param[0] = IMWRITE_JPEG_QUALITY;
  param[0] = 10;
  vector<uchar> buff;

  string data;
  for (Mat frame : pacote->frames){
    imencode(".jpg", frame, buff, param);
    unsigned char* convertido = &buff[0];
    data.append("\"");
    data.append(base64_encode(convertido, buff.size()));
    data.append("\"");
    data.append(",");
  }
  data.pop_back(); // eliminar ultimo ,
  ss << "{\n\"id_user\":\"" << pacote->id_user << "\",\n"
         << "\"id_camera\":\"" << pacote->id_camera << "\",\n"
         << "\"timestamp\":\"" << pacote->timestamp << "\",\n"
         << "\"dados\":[" << data << "]\n}";

  memcpy(buf, ss.str().c_str(), ss.str().size());
  ((unsigned char*)buf)[ss.str().size()] = '\0';
  return ss.str().size();
}

Packet* json_to_packet(void* buf){
  json_object *raw_obj;
  raw_obj = json_tokener_parse((const char*)buf);

  json_object *id_user_json = json_object_object_get(raw_obj, "id_user");
  json_object *id_camera_json = json_object_object_get(raw_obj, "id_camera");
  json_object *timestamp_json = json_object_object_get(raw_obj, "timestamp");
  json_object *dados_json = json_object_object_get(raw_obj, "dados");


  int id_user = (stoi(json_object_get_string(id_user_json)));
  int id_camera = (stoi(json_object_get_string(id_camera_json)));
  // vamos ignorar isto por enquanto time_t timestamp =
  int arraylen = json_object_array_length(dados_json);
  vector<Mat> frames;
  Mat frame;
  json_object * medi_array_obj;
  for(int i = 0; i < arraylen; i++){

    medi_array_obj = json_object_array_get_idx(dados_json, i);
    string aux = json_object_get_string(medi_array_obj);
    aux += '\0';
    std::string det(base64_decode(aux));
    vector<unsigned char> raw_vec(det.begin(), det.end());
    imdecode(raw_vec,1, &frame);
    frames.push_back(frame.clone());
    json_object_put(medi_array_obj);
  }

  //free
  json_object_put(id_user_json);
  json_object_put(id_camera_json);
  json_object_put(timestamp_json);
  //json_object_put(dados_json);

  return new Packet(id_user,id_camera,time(0),frames);
}

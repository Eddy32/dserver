
///////////
#include <zmq.h>
#include <iostream>
#include <cassert>
#include <csignal>
#include <pthread.h>
#include "share_queue.h"
#include "packet.hpp"

// ZMQ
void *sock_pull;
void *sock_push;


// ShareQueue
SharedQueue<Packet*> frame_queue;

// pool
//Frame_pool *frame_pool;

// signal
volatile bool exit_flag = false;
void sig_handler(int s)
{
  exit_flag = true;
}



////////////
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

///////////












void *recv_in_thread(void *ptr)
{
  int recv_json_len;
  unsigned char json_buf[JSON_BUFF_SIZE];
  //Frame frame;

  while(!exit_flag) {
    recv_json_len = zmq_recv(sock_pull, json_buf, JSON_BUFF_SIZE, ZMQ_NOBLOCK);

    if (recv_json_len > 0) {
      Packet* packet = json_to_packet(json_buf);
#ifdef DEBUG
     // std::cout << "Ventilator | Recv From Client | SEQ : " << frame.seq_buf 
       // << " LEN : " << frame.msg_len << std::endl;
       printf("Dados RECEBIDOS\n");
#endif
      frame_queue.push_back(packet);
    }
  }
}

void *send_in_thread(void *ptr)
{
  int send_json_len;
  unsigned char json_buf[JSON_BUFF_SIZE];
  //Frame frame;
  Packet* packet;
  while(!exit_flag) {
    if (frame_queue.size() > 0) {
      packet = frame_queue.front();
      frame_queue.pop_front();

#ifdef DEBUG
   //   std::cout << "Ventilator | Send To Worker | SEQ : " << frame.seq_buf 
    //    << " LEN : " << frame.msg_len << std::endl;
    printf("Dados ENVIADOS\n");
#endif
      
      send_json_len = packet_to_json(json_buf, packet);
      printf("Size buf: %d Size data: %d\n\n",JSON_BUFF_SIZE,send_json_len);
      zmq_send(sock_push, json_buf, send_json_len, 0);

      //frame_pool->free_frame(frame);
    }
  }
}
int main()
{
  // signal
  std::signal(SIGINT, sig_handler);
  // ZMQ
  int ret;
  void *context = zmq_ctx_new(); 
  sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_bind(sock_pull, "tcp://*:5575");
  assert(ret != -1);

  sock_push = zmq_socket(context, ZMQ_PUSH);
  ret = zmq_bind(sock_push, "ipc://unprocessed");
  assert(ret != -1);

  // frame_pool
  //frame_pool = new Frame_pool();

  // Thread
  pthread_t recv_thread;
  if (pthread_create(&recv_thread, 0, recv_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_t send_thread;
  if (pthread_create(&send_thread, 0, send_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_detach(send_thread);
  pthread_detach(recv_thread);

  while(!exit_flag);

  //delete frame_pool;
  zmq_close(sock_pull);
  zmq_close(sock_push);
  zmq_ctx_destroy(context);
}

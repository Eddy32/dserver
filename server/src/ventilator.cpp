
///////////
#include <zmq.h>
#include <iostream>
#include <cassert>
#include <csignal>
#include <pthread.h>
#include <fstream>
#include "share_queue.h"
#include "packet.hpp"
#include <opencv2/opencv.hpp>	
// ZMQ
void *sock_pull;
void *sock_push;


// ShareQueue
SharedQueue<Packet> frame_queue;

// pool
//Frame_pool *frame_pool;

// signal
volatile bool exit_flag = false;
void sig_handler(int s)
{
  exit_flag = true;
}




void *recv_in_thread(void *ptr)
{
  int recv_json_len;
  unsigned char* json_buf = (unsigned char *) malloc(sizeof(unsigned char)*15360000);//[JSON_BUFF_SIZE];
  //Frame frame;

  while(!exit_flag) {
    recv_json_len = zmq_recv(sock_pull, json_buf, JSON_BUFF_SIZE, ZMQ_NOBLOCK);

    if (recv_json_len > 0) {
      Packet* packet = json_to_packet(json_buf);

      ///

      std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 50 };
      int i = 0;
      /*
      for(cv::Mat mat: packet->frames){
        i++;
        printf("frame: %d",i);
        resize(mat, mat, cv::Size(640, 480));

        std::vector<unsigned char> res_vec;
        cv::imencode(".jpg", mat, res_vec, param);
        std::ofstream lelee ("test" + std::to_string(i)+ ".jpg", std::ios::out | std::ios::app | std::ios::binary);
        const char* a = reinterpret_cast<const char*>(&res_vec[0]);
        lelee.write(a,res_vec.size());
        
        
      }
      */
      ///

#ifdef DEBUG
       printf("Dados RECEBIDOS\n");
#endif
      frame_queue.push_back(*packet);
    }
  }
}

void *send_in_thread(void *ptr)
{
  int send_json_len;
  unsigned char* json_buf = (unsigned char *) malloc(sizeof(unsigned char)*15360000);//[JSON_BUFF_SIZE];
  //Frame frame;
  Packet* packet;
  Packet packs;
  while(!exit_flag) {
    if (frame_queue.size() > 0) {
      packs = (frame_queue.front());
      frame_queue.pop_front();
      packet = &packs;
     
      ///
      /*
      std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 50 };
      int i = 0;
      
      for(cv::Mat mat: packet->frames){
        i++;
        printf("frame: %d",i);
        resize(mat, mat, cv::Size(640, 480));

        std::vector<unsigned char> res_vec;
        cv::imencode(".jpg", mat, res_vec, param);
        std::ofstream lelee ("test" + std::to_string(i)+ ".jpg", std::ios::out | std::ios::app | std::ios::binary);
        const char* a = reinterpret_cast<const char*>(&res_vec[0]);
        lelee.write(a,res_vec.size());
        
        
      }
      */
      ///



#ifdef DEBUG
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

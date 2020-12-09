#include <zmq.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <csignal>
#include <assert.h>
#include <tbb/concurrent_hash_map.h>
#include <opencv2/opencv.hpp>
#include <fstream>
#include "share_queue.h"
#include "frame.hpp"
#include "packet.hpp"

// ZMQ
void *sock_pull;
void *sock_pub;

// ShareQueue
//tbb::concurrent_hash_map<int, Frame> frame_map;
SharedQueue<Packet> processed_frame_queue;

// pool
Frame_pool *frame_pool;

// signal
volatile bool exit_flag = false;
void sig_handler(int s)
{
  exit_flag = true;
}

void *recv_in_thread(void *ptr)
{
  int recv_json_len;
  unsigned char json_buf[JSON_BUFF_SIZE];
  cv::VideoWriter writer;
  
  int idV =1;
  while(!exit_flag) {
    recv_json_len = zmq_recv(sock_pull, json_buf, JSON_BUFF_SIZE, ZMQ_NOBLOCK);

    if (recv_json_len > 0) {
       Packet* packet = json_to_packet(json_buf);
      //printf("NUMERO DE FRAMES: %d",packet->frames.size());
      //printf("2\n");

      /////
      /*
      int i =0;
      std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 50 };
      for(cv::Mat mat: packet->frames){
        i++;
        printf("frame: %d",i);
        resize(mat, mat, cv::Size(640, 480));

        std::vector<unsigned char> res_vec;
        cv::imencode(".jpg", mat, res_vec, param);
        std::ofstream lelee ("detet" + std::to_string(i)+ ".jpg", std::ios::out | std::ios::app | std::ios::binary);
        const char* a = reinterpret_cast<const char*>(&res_vec[0]);
        lelee.write(a,res_vec.size());
      }
      */
      ////

      //gravar video
      /*
      writer.open("captura" + std::to_string(idV) + ".avi" ,CV_FOURCC('X','V','I','D'), 20, cv::Size(640, 480), true);
      if (!writer.isOpened()) {
      printf("BRO ESTOU COM TRIPS!");
      }
      idV+=1;

      for(cv::Mat mat: packet->frames){
        printf("printing frame %d \n",idV);
        writer.write(mat);
      }
      */
      processed_frame_queue.push_back(*packet);

      ////

#ifdef DEBUG
     // std::cout << "Sink | Recv From Worker | SEQ : " << frame.seq_buf
     //   << " LEN : " << frame.msg_len << std::endl;
#endif
      tbb::concurrent_hash_map<int, Frame>::accessor a;
      /*while(1)
      {
        if(frame_map.insert(a, atoi((char *)frame.seq_buf))) {
          a->second = frame;
          break;
        }
      }
      */
    }
  }
}

void *send_in_thread(void *ptr)
{
  int send_json_len;
  unsigned char json_buf[JSON_BUFF_SIZE];
  Packet* packet;
  Packet packs;

  while(!exit_flag) {
    if (processed_frame_queue.size() > 0) {
      packs = (processed_frame_queue.front());
      processed_frame_queue.pop_front();
      packet = &packs;
     /* 
      frame = processed_frame_queue.front();
      processed_frame_queue.pop_front();

#ifdef DEBUG
      std::cout << "Sink | Pub To Client | SEQ : " << frame.seq_buf
        << " LEN : " << frame.msg_len << std::endl;
#endif

      send_json_len = frame_to_json(json_buf, frame);
      zmq_send(sock_pub, json_buf, send_json_len, 0);

      frame_pool->free_frame(frame);
      */
     send_json_len = packet_to_json(json_buf, packet);
     printf("Size buf: %d Size data: %d\n\n",JSON_BUFF_SIZE,send_json_len);
     zmq_send(sock_pub, json_buf, send_json_len, 0);

    }
  }
}


int main()
{
  printf("CHANGE");
  // ZMQ
  int ret;
  void *context = zmq_ctx_new();

  sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_bind(sock_pull, "ipc://processed");
  assert(ret != -1);

  sock_pub = zmq_socket(context, ZMQ_PUB);
  ret = zmq_bind(sock_pub, "tcp://*:5570");
  assert(ret != -1);

  // frame_pool
  //frame_pool = new Frame_pool(5000);

  // Thread
  pthread_t recv_thread;
  if (pthread_create(&recv_thread, 0, recv_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_t send_thread;
  if (pthread_create(&send_thread, 0, send_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_detach(send_thread);
  pthread_detach(recv_thread);
  
  // frame 
  //Frame frame;
  volatile int track_frame = 1;
  
  while(!exit_flag) {
    /*
    if (!frame_map.empty()) {
      tbb::concurrent_hash_map<int, Frame>::accessor c_a;

      if (frame_map.find(c_a, (const int)track_frame))
      {
        frame = (Frame)c_a->second;
        while(1) {
          if (frame_map.erase(c_a))
            break;
        }

        processed_frame_queue.push_back(frame);
        track_frame++;
      }
    }
      */
  }

  //delete frame_pool;

  zmq_close(sock_pull);
  zmq_close(sock_pub);
  zmq_ctx_destroy(context);
  return 0;
}

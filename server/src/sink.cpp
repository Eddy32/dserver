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
#include "include/awsdoc/s3/s3_examples.h"




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
  unsigned char* json_buf = (unsigned char *) malloc(sizeof(unsigned char)*JSON_BUFF_SIZE);//[JSON_BUFF_SIZE];
  cv::VideoWriter writer;
  
  int idV =1;
  while(!exit_flag) {
    recv_json_len = zmq_recv(sock_pull, json_buf, JSON_BUFF_SIZE, ZMQ_NOBLOCK);

    if (recv_json_len > 0) {
       Packet* packet = json_to_packet(json_buf);

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

      //gravar video
      
      writer.open("captura" + std::to_string(idV) + ".mov" ,CV_FOURCC('m','p','4','v'), 20, cv::Size(640, 480), true);
      if (!writer.isOpened()) {
      printf("BRO ESTOU COM TRIPS!");
      }
      

      for(cv::Mat mat: packet->frames){
        printf("printing frame %d \n",idV);
        writer.write(mat);
      }

      const std::string bucket_name = "skeyestreammedia";
        const std::string object_name = "captura" + std::to_string(idV) + ".mov";
        const std::string region = "eu-west-3";
        const std::string path = "cams/08/01/20/";

        if (!AwsDoc::S3::PutObject(bucket_name, object_name, region, path)) {

            printf("Nao dei upload para o bucket #ohmaninho");
        }

      idV++;

      processed_frame_queue.push_back(*packet);

      ////

#ifdef DEBUG
     // std::cout << "Sink | Recv From Worker | SEQ : " << frame.seq_buf
     //   << " LEN : " << frame.msg_len << std::endl;
#endif
      tbb::concurrent_hash_map<int, Frame>::accessor a;
    }
  }
  free(json_buf);
}

void *send_in_thread(void *ptr)
{
  int send_json_len;
  unsigned char* json_buf = (unsigned char *) malloc(sizeof(unsigned char)*JSON_BUFF_SIZE);//[JSON_BUFF_SIZE];
  Packet* packet;
  Packet packs;

  while(!exit_flag) {
    if (processed_frame_queue.size() > 0) {
      packs = (processed_frame_queue.front());
      processed_frame_queue.pop_front();
      packet = &packs;
     send_json_len = packet_to_json(json_buf, packet);
     printf("Size buf: %d Size data: %d\n\n",JSON_BUFF_SIZE,send_json_len);
     zmq_send(sock_pub, json_buf, send_json_len, 0);

    }
  }
  //free(json_buf);
}


int main()
{
  printf("CHANGE");
  // ZMQ

  AwsDoc::S3::init();

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
    //live laugh love
  }

  //delete frame_pool;

  zmq_close(sock_pull);
  zmq_close(sock_pub);
  zmq_ctx_destroy(context);
  return 0;
}

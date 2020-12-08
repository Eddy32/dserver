#include <zmq.h>
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <assert.h>
#include <pthread.h>
#include <csignal>
#include "share_queue.h"
#include "args.hpp"
#include "yolo_v2_class.hpp"
#include "DetectorInterface.hpp"
#include "pose_detector.hpp"
#include "yolo_detector.hpp"
#include <opencv2/opencv.hpp>			
#include "frame.hpp"
#include "frame.hpp"
#include "packet.hpp"
#include <fstream>

// ZMQ
void *sock_pull;
void *sock_push;

// ShareQueue
SharedQueue<Packet> unprocessed_frame_queue;
SharedQueue<Packet> processed_frame_queue;
//SharedQueue<Packet*> test;

// pool
// Frame_pool *frame_pool;

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
  

  while(!exit_flag) {
    recv_json_len = zmq_recv(sock_pull, json_buf, JSON_BUFF_SIZE, ZMQ_NOBLOCK);
    if (recv_json_len > 0) {
      Packet* packet = json_to_packet(json_buf);
      //printf("NUMERO DE FRAMES: %d",packet->frames.size());
      //printf("2\n");

      /////
      int i =0;
      std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 50 };
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
    /////////


#ifdef DEBUG
      //std::cout << "Worker | Recv From Ventilator | SEQ : " << frame.seq_buf 
      //  << " LEN : " << frame.msg_len << std::endl;
#endif
      unprocessed_frame_queue.push_back(*packet);
    printf("3\n");
    }
  }
}

void *send_in_thread(void *ptr)
{
  int send_json_len;
  unsigned char json_buf[JSON_BUFF_SIZE];
  Packet* packet;

  while(!exit_flag) {
    if (processed_frame_queue.size() > 0) {
      printf("4 %d\n", processed_frame_queue.size());
      packet = &(processed_frame_queue.front());

      processed_frame_queue.pop_front();

#ifdef DEBUG
   //   std::cout << "Worker | Send To Sink | SEQ : " << frame.seq_buf
    //    << " LEN : " << frame.msg_len << std::endl;
#endif
      printf("5\n");
      //send_json_len = packet_to_json(json_buf, packet);
      //zmq_send(sock_push, json_buf, send_json_len, 0);
      printf("6\n");
     
    }
  }
}


int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "usage: %s <cfg> <weights> <names> [-pose] [-gpu GPU_ID] [-thresh THRESH]\n", argv[0]);
    return 0;
  }

  // signal
  std::signal(SIGINT, sig_handler);

  const char *cfg_path = argv[1];
  const char *weights_path = argv[2];
  const char *names_path = argv[3];
  int gpu_id = find_int_arg(argc, argv, "-gpu", 0);
  int pose_flag = find_arg(argc, argv, "-pose");
  float thresh = find_float_arg(argc, argv, "-thresh", 0.2);
  fprintf(stdout, "cfg : %s, weights : %s, names : %s, gpu-id : %d, thresh : %f\n", 
      cfg_path, weights_path, names_path, gpu_id, thresh);

  // opencv
  std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 50 };

  // ZMQ
  int ret;

  void *context = zmq_ctx_new();

  sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_connect(sock_pull, "ipc://unprocessed");
  assert(ret != -1);

  sock_push = zmq_socket(context, ZMQ_PUSH);
  ret = zmq_connect(sock_push, "ipc://processed");
  assert(ret != -1);

  // frame__pool
  //frame_pool = new Frame_pool(5000);
  new CMemPool(5000, 100);
  new CMemPool(5000, 76800);
  new CMemPool(5000, 25600);
  // Thread
  pthread_t recv_thread;
  if (pthread_create(&recv_thread, 0, recv_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_t send_thread;
  if (pthread_create(&send_thread, 0, send_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_detach(send_thread);
  pthread_detach(recv_thread);

  // darkent
  DetectorInterface *detector = nullptr;
  if (pose_flag) {
    detector = new PoseDetector(cfg_path, weights_path, gpu_id);
  }
  else {
    detector = new YoloDetector(cfg_path, weights_path, names_path, gpu_id);
  }
  //PoseDetector pose_detector(cfg_path, weights_path, gpu_id);

 // frame
  //Frame frame;
  Packet* packet;
  int frame_len;
  int frame_seq;
  unsigned char *frame_buf_ptr;

  auto time_begin = std::chrono::steady_clock::now();
  auto time_end = std::chrono::steady_clock::now();
  double det_time;
  int msg = 0;
  int iframe = 0;
  while(!exit_flag) {
    // recv from ven
    if (unprocessed_frame_queue.size() > 0) {
      msg+=1;
      packet = &(unprocessed_frame_queue.front());
      unprocessed_frame_queue.pop_front();

      //frame_seq = atoi((const char*)frame.seq_buf);
      //frame_len = frame.msg_len;
      //frame_buf_ptr = frame.msg_buf;

      // darknet
      // unsigned char array -> vector
      std::vector<cv::Mat> matBoxes;
      for(cv::Mat mat: packet->frames){
        printf("MENSAGEM: %d, Frame: %d",msg,iframe);
        iframe+=1;
        //std::vector<unsigned char> raw_vec(frame_buf_ptr, frame_buf_ptr + frame_len);
        resize(mat, mat, cv::Size(640, 480));
      // vector -> mat
      //cv::Mat raw_mat = cv::imdecode(cv::Mat(raw_vec), 1);

      // detect
      time_begin = std::chrono::steady_clock::now();
      detector->detect(mat, thresh);
      time_end = std::chrono::steady_clock::now();
      det_time = std::chrono::duration <double, std::milli> (time_end - time_begin).count();

#ifdef DEBUG
      //n std::cout << "Darknet | Detect | SEQ : " << frame.seq_buf << " Time : " << det_time << "ms" << std::endl;
#endif

      // detect result to jsons
      //std::string det_json = detector->det_to_json(frame_seq);
      //frame.det_len = det_json.size();
      //memcpy(frame.det_buf, det_json.c_str(), frame.det_len);
      //frame.det_buf[frame.det_len] = '\0';

      // detect result (bounding box OR Skeleton) draw
      detector->draw(mat);
      matBoxes.push_back(mat.clone());
      // mat -> vector
      std::vector<unsigned char> res_vec;
      cv::imencode(".jpg", mat, res_vec, param);
      std::ofstream lelee ("test" + std::to_string(msg)+ "/" + std::to_string(iframe) + ".jpg", std::ios::out | std::ios::app | std::ios::binary);
      const char* a = reinterpret_cast<const char*>(&res_vec[0]);
      lelee.write(a,res_vec.size());
      }
      Packet* packetProcessed = new Packet(packet->id_user,packet->id_camera,packet->timestamp,matBoxes);
      // vector -> frame array
      //frame.msg_len = res_vec.size();
      //std::copy(res_vec.begin(), res_vec.end(), frame.msg_buf);

      // push to processed frame_queue
      processed_frame_queue.push_back(*packetProcessed);
      
    }
  }
  
  //delete frame_pool;
  zmq_close(sock_pull);
  zmq_close(sock_push);
  zmq_ctx_destroy(context);

  return 0;
}

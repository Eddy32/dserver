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

#include <iostream>
#include <string>
#include <curl/curl.h>
#include <time.h>
#include<string>



// ZMQ
void *sock_pull;
void *sock_pub; 

void *stream_pub;


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
  time_t theTime;
  struct tm *aTime;
  int recv_json_len;
  unsigned char* json_buf = (unsigned char *) malloc(sizeof(unsigned char)*JSON_BUFF_SIZE);//[JSON_BUFF_SIZE];
  cv::VideoWriter writer;
  std::string filename;
  std::string outputname;
  std::string comand;
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
      filename = "cap" + std::to_string(idV) + ".avi";
      outputname = "cap" + std::to_string(idV) + ".mp4";
      comand = "ffmpeg -i " +  filename + " " +  outputname + " -y";

      writer.open(filename ,CV_FOURCC('X','V','I','D'), 20, cv::Size(640, 480), true);
      if (!writer.isOpened()) {
      printf("BRO ESTOU COM TRIPS!");
      }
      

      for(cv::Mat mat: packet->frames){
        //printf("printing frame %d \n",idV);
        writer.write(mat);
      }
      writer.release();

      system(comand.c_str());

      ////

      ///Enviar para aws -> s3 bucket
      
      time_t theTime = time(NULL);
      struct tm *aTime = localtime(&theTime);


      const std::string bucket_name = "skeyestreammedia";
      const std::string object_name = outputname;
      const std::string region = "eu-west-3";
      const std::string path = "cams/" + packet->id_camera + "/" + std::to_string(aTime->tm_year) + "/" + std::to_string((aTime->tm_mon)+1) + "/" + std::to_string(aTime->tm_mday)+"/" ;

      if (!AwsDoc::S3::PutObject(bucket_name, object_name, region, path)) {

          printf("Nao dei upload para o bucket ");
      }
      
      //// Post da deteção
      CURLcode ret;
      CURL *hnd;
      struct curl_slist *slist1;
      std::string idUser = packet->id_user;
      std::string idCamera = packet->id_camera;
      std::string timestamp = packet->timestamp;
      std::string classes = "["
      int nClasses = (packet->classes).size();
      int counter = 1;
      for(string classfound: packet->classes){
        if(counter!=nClasses)
          classes = classes + "\"" + classfound + "\",";
        else
          classes = classes + "\"" + classfound + "\""; 
      }
      classes = classes + "]";
      cout << "CLASSES" << classes << "\n\n\n\n\n\n"
      std::string url = "https://skeyestreammedia.s3.eu-west-3.amazonaws.com/" + path + object_name; 
      std::string jsonstr = "{\"idUser\": \"" + idUser +
      "\",\"idCamera\": \""+ idCamera +
            "\",\"timestamp\":\""+ timestamp +
            "\",\"classes\": " + classes  +
            ",\"urlVideo\": \""+   url   +
            "\"}";

      printf("STRING: %s \n\n\n\n\n\n", jsonstr);

      slist1 = NULL;
      slist1 = curl_slist_append(slist1, "Content-Type: application/json");

      hnd = curl_easy_init();
      curl_easy_setopt(hnd, CURLOPT_URL, "http://skeye-backend.herokuapp.com/notifications/yolo");
      curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
      curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, jsonstr.c_str());
      curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.38.0");
      curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
      curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
      curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
      curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

      ret = curl_easy_perform(hnd);

      curl_easy_cleanup(hnd);
      hnd = NULL;
      curl_slist_free_all(slist1);
      slist1 = NULL;
      

    //stream
  


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
  int i = 18;
  while(!exit_flag) {
    if (processed_frame_queue.size() > 0) {
      packs = (processed_frame_queue.front());
      processed_frame_queue.pop_front();
      packet = &packs;

      
      
      
     for(cv::Mat mat: packet->frames){
        
        
        int height = mat.rows;
        int width = mat.cols;
        printf("ALT: %d + LARRG: %d",height,width);
        cv::vector<uchar> buffer;
        cv::vector<uchar> topic ;
        cv::imencode(".jpg", mat, buffer);
        printf("SIZE: %d  ----\n",buffer.size());
        //zmq_send(stream_pub,"20", 2, ZMQ_SNDMORE);
        zmq_send(stream_pub, buffer.data(), buffer.size(), 0);
      }

     send_json_len = packet_to_json(json_buf, packet);
     printf("Size buf: %d Size data: %d\n\n",JSON_BUFF_SIZE,send_json_len);
     //zmq_send(sock_pub, json_buf, send_json_len, 0);


  


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
  ret = zmq_bind(sock_pull, "tcp://*:45645");
  assert(ret != -1);

  sock_pub = zmq_socket(context, ZMQ_PUB);

  stream_pub = zmq_socket(context,ZMQ_PUB);
  ret = zmq_connect(stream_pub,"tcp://127.0.0.1:47777"); 

  ret = zmq_bind(sock_pub, "tcp://*:5571");
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
  zmq_close(stream_pub);
  zmq_ctx_destroy(context);
  return 0;
}

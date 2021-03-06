// "hi" 
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <vector>
#include <math.h>
#include <time.h>
#include <thread>
#include <mutex>

using std::mutex;
using std::thread;
using std::string;
using std::cout;
using std::endl;
using std::vector;

uint64_t pack754(double f, unsigned bits, unsigned expbits)
{
    long double fnorm;
    int shift;
    long long sign, exp, significand;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (f == 0.0) return 0; // get this special case out of the way

    // check sign and begin normalization
    if (f < 0) { sign = 1; fnorm = -f; }
    else { sign = 0; fnorm = f; }

    // get the normalized form of f and track the exponent
    shift = 0;
    while(fnorm >= 2.0) { fnorm /= 2.0; shift++; }
    while(fnorm < 1.0) { fnorm *= 2.0; shift--; }
    fnorm = fnorm - 1.0;

    // calculate the binary form (non-float) of the significand data
    significand = fnorm * ((1LL<<significandbits) + 0.5f);

    // get the biased exponent
    exp = shift + ((1<<(expbits-1)) - 1); // shift + bias

    // return the final answer
    return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
}

double unpack754(uint64_t i, unsigned bits, unsigned expbits)
{
    long double result;
    long long shift;
    unsigned bias;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (i == 0) return 0.0;

    // pull the significand
    result = (i&((1LL<<significandbits)-1)); // mask
    result /= (1LL<<significandbits); // convert back to float
    result += 1.0f; // add the one back on

    // deal with the exponent
    bias = (1<<(expbits-1)) - 1;
    shift = ((i>>significandbits)&((1LL<<expbits)-1)) - bias;
    while(shift > 0) { result *= 2.0; shift--; }
    while(shift < 0) { result /= 2.0; shift++; }

    // sign it
    result *= (i>>(bits-1))&1? -1.0: 1.0;

    return result;
}

unsigned char * serialize_uint_64(unsigned char *buffer, uint64_t value)
{
  buffer[0] = value >> 56;
  buffer[1] = value >> 48;
  buffer[2] = value >> 40;
  buffer[3] = value >> 32;
  buffer[4] = value >> 24;
  buffer[5] = value >> 16;
  buffer[6] = value >> 8;
  buffer[7] = value;
  return buffer + 8;
}

unsigned char* deserialize_uint_64(unsigned char *buffer, uint64_t* value)
{
  *value = (uint64_t)buffer[0]<<56;
  *value = *value + ((uint64_t)buffer[1]<<48);
  *value = *value + ((uint64_t)buffer[2]<<40);  
  *value = *value + ((uint64_t)buffer[3]<<32);
  *value = *value + ((uint64_t)buffer[4]<<24);
  *value = *value + ((uint64_t)buffer[5]<<16);
  *value = *value + ((uint64_t)buffer[6]<<8); 
  *value = *value + ((uint64_t)buffer[7]);    
  return buffer + 8;
}

#pragma pack(1)

struct x_and_u
{
  double x;
  double u;
  double z;
};

#pragma pack(0)

#pragma pack(1)

struct admm_z
{
  double z;
};

#pragma pack(0)

void send_msg_struct(int new_sd, unsigned char* buffer, admm_z msg_send, int len)
{
  unsigned char* point;
  ssize_t bytes_sent;
  uint64_t temp_64;
  temp_64 = pack754(msg_send.z,64,11);
  point = serialize_uint_64(buffer,temp_64); 
  bytes_sent = send(new_sd, buffer, len, 0);
  cout<<"\nbytes_sent: "<<bytes_sent<<"\n";
}

void send_msg_int(int new_sd, int value)
{
  unsigned char* point;
  ssize_t bytes_sent;
  int len = 8;
  unsigned char buffer[8];
  point = serialize_uint_64(&buffer[0],value); 
  bytes_sent = send(new_sd, buffer, len, 0);
}

void send_msg_struct_cl(int socketfd, int i, unsigned char* buffer, x_and_u msg_send, int len)
{
  unsigned char* point;
  ssize_t bytes_sent;
  uint64_t temp_64;
  temp_64 = pack754(msg_send.x,64,11);
  point = serialize_uint_64(buffer,temp_64);
  temp_64 = pack754(msg_send.u,64,11);
  point = serialize_uint_64(point,temp_64); 
  bytes_sent = send(socketfd, buffer, len, 0);
  cout<<"\nFrom :"<<i<<" bytes_sent: "<<bytes_sent<<"\n";
}

void recv_msg_int(int socketfd, int* msg_recv)
{
  unsigned char* point;
  ssize_t bytes_recv;
  uint64_t temp_64;
  unsigned char buffer[8];
  int len = 8;
  bytes_recv = recv(socketfd, &buffer[0], len, 0);
  point = deserialize_uint_64(buffer, &temp_64);
  *msg_recv = temp_64;
  //cout<<"msg_recv: "<<*msg_recv<<"\n";
}

void recv_msg_struct_cl(int socketfd, int i, unsigned char* buffer, admm_z* msg_recv, int len)
{
  unsigned char* point;
  ssize_t bytes_recv;
  uint64_t temp_64;
  bytes_recv = recv(socketfd, buffer, len, 0);
  point = deserialize_uint_64(buffer, &temp_64);
  cout<<"From "<<i<<", bytes received: "<<bytes_recv<<endl;
  msg_recv->z = unpack754(temp_64,64,11);
}

void recv_msg_struct(int new_sd, int count, unsigned char* buffer, x_and_u* msg_recv, int len)
{
  unsigned char* point;
  ssize_t bytes_recv;
  uint64_t temp_64;
  bytes_recv = recv(new_sd, buffer, len, 0);
  point = deserialize_uint_64(buffer, &temp_64);
  cout<<"bytes received: "<<bytes_recv<<endl;
  msg_recv->x = unpack754(temp_64,64,11);
  point = deserialize_uint_64(point,&temp_64); 
  msg_recv->u = unpack754(temp_64,64,11);
  cout<<"\nFrom client "<<count<<":\n";
  cout<<"msg x: "<<msg_recv->x<<"\n";
  cout<<"msg u: "<<msg_recv->u<<"\n"; 
}

void recv_msg_string(int new_sd, string& s)
{
 ssize_t bytes_received;
 char incoming_buffer[100];
 bytes_received = recv(new_sd, incoming_buffer, 100, 0);
 string s1(incoming_buffer);
 s = s1;
 cout<<"\nbytes received: "<<bytes_received<<"\n";
}

void create_socket_server(const char* s, char& connect_type, int& sockets_server, mutex& mtx)
{
 int status;
 struct addrinfo host_info;       
 struct addrinfo *host_info_list; 
   
 memset(&host_info, 0, sizeof host_info);
 
 cout<<"Setting up the structs..."<<endl;
 
 host_info.ai_family = AF_UNSPEC;
 host_info.ai_flags = AI_PASSIVE;     
 host_info.ai_socktype = SOCK_STREAM; 
 
 status = getaddrinfo(s, "5532", &host_info, &host_info_list);

 if (status != 0)  
 {
   cout<<"getaddrinfo error: " << gai_strerror(status);
 }
 cout<<"\nCreating a socket \n";
 int socketfd;
 socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
 if (socketfd == -1)
 {
   cout<<"socket error \n";
 }
 cout<<"Binding socket..."<<endl;

 int yes = 1;
 status = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
 status = bind(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
 if (status == -1)  
 {
   cout<<"bind error"<<endl;
 } 
 cout << "Listening for connections..."<<endl;
 //cout<<"before listen.\n";
 status =  listen(socketfd, 5);
 //cout<<"after listen.\n";
 if (status == -1)  
 {
   cout<< "listen error"<<endl;
 }
 //cout<<"after if.\n";
 int new_sd;
 struct sockaddr_storage their_addr;
 socklen_t addr_size = sizeof(their_addr);
 //cout<<"before accept.\n";
 new_sd = accept(socketfd, (struct sockaddr *)&their_addr, &addr_size);
 if (new_sd == -1)
 {
    cout << "listen error" << std::endl ;
 }
 else
 {
   cout << "Connection accepted. Using new socketfd : "<<new_sd<<endl;
   //mtx.lock();
   connect_type = 's';
   //mtx.unlock();
 }
 sockets_server = new_sd;
}

void create_socket_client(const char* s, char& connect_type, int& socket_client, mutex& mtx)
{
  int status;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;

  memset(&host_info, 0, sizeof host_info);

  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  status = getaddrinfo(s, "5532", &host_info, &host_info_list);
  cout<<"status: "<<status<<endl;
  if (status != 0)
  {
    cout<<"getaddrinfo error"<<endl<<gai_strerror(status);
  }
  cout<<"Creating a socket \n";
  int socketfd;
  socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
  if (socketfd == -1)
  {
    cout<<"socket error \n";
  }
  bool error_once = 0;
  while(1)
  {
  status =  connect(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1 && error_once == 0)
  {
    cout<<"connect error \n";
    error_once = 1;
  }
  else if (status == 0)
  {
    //mtx.lock();
    connect_type = 'c';
    //mtx.unlock();
    cout << "send()ing message..."  <<endl;
    cout<<"status: "<<status<<"\n";
    break;
  }
  }
  socket_client = socketfd;
}

struct attributes
{
  char* ip;
  const char* inter_this;
  const char* inter_neigh;
  int neigh_ind; 
  int node_deg;
  bool active;
  bool bridge;
  char connect_type;
};

void initialize_address_attr(const char* inter_this, const char* inter_neigh, attributes& neighbor_attr, char* ip_,
     int index, int degree, bool act, bool bridge_act)
{
  //addr = inter_const;
  neighbor_attr.inter_this = inter_this;
  neighbor_attr.inter_neigh = inter_neigh;
  neighbor_attr.ip = ip_;
  neighbor_attr.neigh_ind = index;
  neighbor_attr.node_deg = degree;
  neighbor_attr.active = act;
  neighbor_attr.bridge = bridge_act;
}

void initialize_all_addresses(vector<attributes>& all_neighbors_attr)
{
  //adresses[0] = "172.16.0.1";
  //adresses[0] = "172.16.0.5";
  //adresses[1] = "172.16.0.18";
  //all_neighbors_attr[0].inter = "172.16.0.5";
  //all_neighbors_attr[1].inter = "172.16.0.18";
  //all_neighbors_attr[0].ip = "147.72.248.20";
  //addresses[0] = "172.16.0.5";
  initialize_address_attr("172.16.0.5", "172.16.0.6", all_neighbors_attr[0], "147.72.248.11", 0, 1, 1, 0);
  //initialize_address_attr("172.16.0.9", "172.16.0.14", all_neighbors_attr[1], "147.72.248.17", 0, 1, 0);
}

void send_msg_string(int socketfd, char* msg)
{
  int len;
  ssize_t bytes_sent;
  len = strlen(msg);
  bytes_sent = send(socketfd, msg, len, 0);
}

void check_neighbors(vector<int>& temp_vec, string& s, vector<attributes>& all_neighbors_attr)
{
  for(int k = 0; k < all_neighbors_attr.size(); k++)
  {
    string s2(all_neighbors_attr[k].ip);
    if (s == s2 && (all_neighbors_attr[k].active == 1 || all_neighbors_attr[k].active == 0))
    { 
      cout<<"\ninside if, ip: "<<s2<<"\n";
      temp_vec.push_back(k);
      break;
    }
  }
}

void send_bridge_neighbors(vector<attributes>& all_neighbors_attr, vector<int>& sockets, bool this_bridge)
{
  if (this_bridge == 1)
  {
    int i, no_of_neigh, total_active_neigh = 0;
    no_of_neigh = all_neighbors_attr.size();
    for (i = 0; i < no_of_neigh; i++)
    {
      if (all_neighbors_attr[i].active == 1)
      { total_active_neigh = total_active_neigh + 1; }
    }
    for (i = 0; i < no_of_neigh; i++)
    {
      if (all_neighbors_attr[i].active == 1)
      {
        cout<<"\ntotal_active_neigh: "<<total_active_neigh<<"\n";
        send_msg_int(sockets[i],no_of_neigh);
        for (int j = 0; j < no_of_neigh; j++)
        {
         //if (all_neighbors_attr[j].active == 1)
         send_msg_string(sockets[i], all_neighbors_attr[j].ip); 
        }
     } 
   }
 }
}

void receive_bridge_neighbors(vector<attributes>& all_neighbors_attr, vector<int>& sockets, vector< vector<int> >& bridge_neighbors)
{
  int i;
  for (i = 0; i < all_neighbors_attr.size(); i++)
  {
    if (all_neighbors_attr[i].bridge == 1 && all_neighbors_attr[i].active == 1)
    {
      vector<int> temp_vec;
      int j, no_of_neigh;
      recv_msg_int(sockets[i], &no_of_neigh);
      cout<<"\nbridge "<<i<<": ";
      cout<<"\nno of neighbors: "<<no_of_neigh<<"\n";
      for (j = 0; j < no_of_neigh; j++)
      {
        ssize_t bytes_recieved;
        char incoming_buffer[13];
        bytes_recieved = recv(sockets[i], incoming_buffer, 13, 0);  // receiving address of bridge node neighbor
        string s(incoming_buffer);
        cout<<"ip: "<<s<<"\n";
        check_neighbors(temp_vec, s, all_neighbors_attr);
      }
      bridge_neighbors.push_back(temp_vec);
    }
  }
}

void create_connections(vector<attributes>& all_neighbors_attr, vector<int>& sockets_server, vector<int>& sockets_client)
{
  int i;
  mutex mtx_1, mtx_2;
  for (i = 0; i < all_neighbors_attr.size(); i++)
  {
    thread first(create_socket_server, all_neighbors_attr[i].inter_this, std::ref(all_neighbors_attr[i].connect_type), std::ref(sockets_server[i]), std::ref(mtx_1));
    thread second(create_socket_client, all_neighbors_attr[i].inter_neigh, std::ref(all_neighbors_attr[i].connect_type), std::ref(sockets_client[i]), std::ref(mtx_2));
    first.join();
    second.join();
  }
}

struct char_8_array
{
  unsigned char buffer[8];
};

struct char_16_array
{
  unsigned char buffer[16];
};

void create_recv_threads_bridge(vector<attributes>& all_neighbors_attr, vector<int>& sockets_server, x_and_u& avg, vector<x_and_u>& msg_recv_vec, vector<thread>& recv_threads)
{
  vector<char_16_array> buffer_recv_vec;
  buffer_recv_vec.resize(all_neighbors_attr.size());
  int i, len_recv = 16, count = 0;;
  for (i = 0; i < all_neighbors_attr.size(); i++)
  {
    if (all_neighbors_attr[i].active == 1)
    {
      count = count + 1;
      recv_threads.push_back(thread(recv_msg_struct, sockets_server[i], count, &buffer_recv_vec[i].buffer[0], &msg_recv_vec[i], len_recv));
    }
  }
}

void join_recv_threads_bridge(vector<thread>& recv_threads)
{
  for (int i = 0; i < recv_threads.size(); i++)
  { recv_threads[i].join(); }
}

void compute_z(vector<attributes>& all_neighbors_attr, vector<x_and_u>& msg_recv_vec, x_and_u& avg, admm_z& msg_send)
{
  int i, client = 0;
  for (i = 0; i < all_neighbors_attr.size(); i++)
  {
    if (all_neighbors_attr[i].active == 1)
    {
      avg.x = avg.x + msg_recv_vec[i].x;
      avg.u = avg.u + msg_recv_vec[i].u;     
      client = client + 1;
    }
  }
  avg.x = avg.x/(client + 1);
  avg.u = avg.u/(client + 1);
  msg_send.z = avg.x + avg.u;
}

void create_send_threads_bridge(vector<attributes>& all_neighbors_attr, vector<thread>& send_threads, vector<int>& sockets_server, vector<char_8_array>& buffer_send_vec,  admm_z msg_send)
{
  int i, len_send = 8;
  //vector<char_8_array> buffer_send_vec;
  //buffer_send_vec.resize(all_neighbors_attr.size());
 for (i = 0; i < all_neighbors_attr.size(); i++)
 {
   if (all_neighbors_attr[i].active == 1)
   {
     send_threads.push_back(thread(send_msg_struct, sockets_server[i], &buffer_send_vec[i].buffer[0], msg_send, len_send));
   }
 }
 //send_thread_ = thred(send_msg_struct, sockets_server[i], &buffer_send_vec[0].buffer[0], msg_send, len_send);
}

void join_send_threads_bridge(vector<thread>& send_threads)
{
  for (int i = 0; i < send_threads.size(); i++)
  { send_threads[i].join(); }
}

void create_send_thread_temp(vector<thread>& send_thread, vector<int>& sockets, vector<char_8_array>& buffer_send, admm_z& msg_send, int len)
{
  send_thread.push_back(thread(send_msg_struct, sockets[0], &buffer_send[0].buffer[0], msg_send, len)); 
}

void join_send_thread_temp(vector<thread>& send_thread)
{
  send_thread[0].join();
}  

void bridge_node_computation(vector<int>& sockets_server, x_and_u& avg, admm_z& msg_send, 
                        vector<attributes>& all_neighbors_attr, bool this_bridge)
{
  if (this_bridge == 1)
  {
    vector<thread> recv_threads;
    vector<thread> send_threads;
    vector<char_8_array> buffer_send_vec;
    int len = 8;
    buffer_send_vec.resize(all_neighbors_attr.size());
    x_and_u msg_recv;
    msg_recv.x = 0.0;
    msg_recv.u = 0.0;
    vector<x_and_u> msg_recv_vec(all_neighbors_attr.size(),msg_recv);
    vector<attributes> attr_recv, attr_send;
    attr_recv = all_neighbors_attr;
    attr_send = all_neighbors_attr;
    //compute_z(all_neighbors_attr, msg_recv_vec, avg, msg_send);
    //cout<<"\nafter compute_z in bridge_node_computation.\n";
    //create_send_threads_bridge(all_neighbors_attr, send_threads, sockets_server, msg_send);
    //cout<<"\nafter create_send_threads_bridge function in bridge_node_computation.\n";
    //join_send_threads_bridge(send_threads);
    //cout<<"\nafter join_send_threads_bridge function in bridge_node_computation.\n";
    create_recv_threads_bridge(attr_recv, sockets_server, avg, msg_recv_vec, recv_threads);
    join_recv_threads_bridge(recv_threads);
    cout<<"\nafter joining recv threads.\n";
    compute_z(all_neighbors_attr, msg_recv_vec, avg, msg_send);
    cout<<"\nafter compute_z in bridge_node_computation.\n";
    create_send_threads_bridge(attr_send, send_threads, sockets_server,  buffer_send_vec, msg_send);
    //vector<thread> send_thread;
    //create_send_thread_temp(send_thread, sockets_server, buffer_send_vec, msg_send, len);
    //join_send_thread_temp(send_thread);
    //thread send_thread(send_msg_struct, sockets_server[0], &buffer_send_vec[0].buffer[0], msg_send, len); 
    //send_thread.join();
    
    //cout<<"\nafter create_send_threads_bridge function in bridge_node_computation.\n";
    join_send_threads_bridge(send_threads);
    cout<<"\nafter join_send_threads_bridge function in bridge_node_computation.\n";
 }
}

int compute_no_of_bridges(vector<attributes>& all_neighbors_attr, bool this_bridge)
{
  int i, no_of_bridges = 0;
  for (i = 0; i < all_neighbors_attr.size(); i++)
  {
    if (all_neighbors_attr[i].bridge == 1)
    {
      no_of_bridges = no_of_bridges + 1;
    }
  }
  if (this_bridge == 1)
  { no_of_bridges = no_of_bridges + 1; }
  return no_of_bridges;
}

void create_recv_threads(vector<attributes>& all_neighbors_attr, vector<int>& sockets_client, 
                         vector<thread>& recv_msg_cl_thread, vector<admm_z>& msg_recv_serv_vec)
{
  int i, count = 0, len_recv_serv = 8;
  vector<char_8_array> buffer_recv_serv_vec;
  buffer_recv_serv_vec.resize(all_neighbors_attr.size());
  cout<<"\ninside create_recv_threads, before for loop.";
  for (i = 0; i < all_neighbors_attr.size(); i++)
  { 
    if (all_neighbors_attr[i].bridge == 1 && all_neighbors_attr[i].active == 1)
    {
     count = count + 1;
     cout<<"\nhere, inside if condition of create_recv_threads.\n";
     recv_msg_cl_thread.push_back( thread(recv_msg_struct_cl, sockets_client[i], count, &buffer_recv_serv_vec[i].buffer[0], 
                       &msg_recv_serv_vec[i], len_recv_serv) );
    }
  }
 cout<<"\nafter create_recv_threads, after for loop.";
}

void join_recv_threads(vector<attributes>& all_neighbors_attr, vector<thread>& recv_msg_cl_thread, vector<admm_z>& msg_recv_serv_vec)
{
  int i, count = 0;
  for (i = 0; i < all_neighbors_attr.size(); i++)
  { 
    if (all_neighbors_attr[i].bridge == 1 && all_neighbors_attr[i].active == 1)
    {
      recv_msg_cl_thread[count].join(); 
      count = count + 1; 
      cout<<"From "<<count<<": ,msg z: "<<msg_recv_serv_vec[i].z<<"\n"; 
    }
  }
}

void create_send_threads(vector<attributes>& all_neighbors_attr, vector<thread>& send_msg_cl_thread, vector<int>& sockets_client, x_and_u& msg_send_serv, vector<double>& u_b)
{
  int i, count = 0, len_send_serv = 16;
  vector<char_16_array> buffer_send_serv_vec;
  buffer_send_serv_vec.resize(all_neighbors_attr.size());
  for (i = 0; i < all_neighbors_attr.size(); i++)
  { 
    if (all_neighbors_attr[i].bridge == 1 && all_neighbors_attr[i].active == 1)
    {
      msg_send_serv.u = u_b[count];
      send_msg_cl_thread.push_back( thread(send_msg_struct_cl, sockets_client[i], count, &buffer_send_serv_vec[i].buffer[0], 
                     msg_send_serv, len_send_serv) );
      count = count + 1;   
    }
  }
}

void join_send_threads(vector<thread>& send_msg_cl_thread)
{
  for (int i = 0; i < send_msg_cl_thread.size(); i++)
  { send_msg_cl_thread[i].join(); }
}

void compute_x_value(vector<attributes>& all_neighbors_attr, int no_of_bridges, double z_value, 
                      x_and_u& msg_send_serv, double sum_u_b, vector<double>& z_b, const double rho, const double v)
{
  double sum_z_b = 0.0;
  int count = 0;
  for (int i = 0; i < all_neighbors_attr.size(); i++)
  {
    if (all_neighbors_attr[i].bridge == 1 && all_neighbors_attr[i].active == 1)
    {
      //z_b[count] = msg_recv_serv_vec[i].z;
      sum_z_b = sum_z_b + z_b[count];
      count = count + 1;
    }
  }
  sum_z_b = sum_z_b + z_value;
  msg_send_serv.x = (2*v + rho*sum_z_b - rho*sum_u_b)/(2+rho*no_of_bridges);
}

void compute_u_values(vector<attributes>& all_neighbors_attr, x_and_u& msg_send_serv, vector<admm_z>& msg_recv_serv_vec, double& sum_u_b, vector<double>& z_b, vector<double>& u_b)
{
  int i, count = 0;
  for (i = 0; i < all_neighbors_attr.size(); i++)
  { 
    if (all_neighbors_attr[i].bridge == 1 && all_neighbors_attr[i].active == 1)
    {
      z_b[count] = msg_recv_serv_vec[i].z;
      u_b[count] = u_b[count] + msg_send_serv.x - z_b[count];
      sum_u_b = sum_u_b + u_b[count];
      count = count + 1;  
    }
  }
}

void normal_node_computation(vector<attributes>& all_neighbors_attr, vector<int>& sockets_client, double z_value, 
    x_and_u& msg_send_serv, vector<admm_z>& msg_recv_serv_vec, double& sum_u_b, bool this_bridge, const double rho, const double v)
{
  int i, no_of_bridges, count = 0;
  double sum_z_b = 0.0;
  no_of_bridges = compute_no_of_bridges(all_neighbors_attr, this_bridge);
  vector<double> u_b(no_of_bridges, 0.0);
  vector<double> z_b(no_of_bridges, 0.0);
  //admm_z msg_recv_serv;
  //msg_recv_serv.z = 0.0;
  //vector<admm_z> msg_recv_serv_vec(all_neighbors_attr.size(),msg_recv_serv);
  vector<thread> recv_msg_cl_thread;
  vector<thread> send_msg_cl_thread;
  compute_u_values(all_neighbors_attr, msg_send_serv, msg_recv_serv_vec, sum_u_b, z_b, u_b);
  compute_x_value(all_neighbors_attr, no_of_bridges, z_value, msg_send_serv, sum_u_b, z_b, rho, v);
  create_send_threads(all_neighbors_attr, send_msg_cl_thread, sockets_client, msg_send_serv, u_b);
  join_send_threads(send_msg_cl_thread);
  create_recv_threads(all_neighbors_attr, sockets_client, recv_msg_cl_thread, msg_recv_serv_vec);
  join_recv_threads(all_neighbors_attr, recv_msg_cl_thread, msg_recv_serv_vec);
  cout<<"\nafter joining create_recv_threads in normal_node_computation.\n";
  sum_u_b = 0.0;
  if (this_bridge == 1)
  {
    u_b[no_of_bridges-1] = u_b[no_of_bridges-1] + msg_send_serv.x - z_value;
    sum_u_b = sum_u_b + u_b[no_of_bridges-1]; 
    msg_send_serv.u = u_b[no_of_bridges-1];
  }
  cout<<"\nafter normal_node_computation.\n";
}

int main ()
{
  clock_t t_1, t_2;
  x_and_u avg, msg_send_serv;
  admm_z msg_send;
  double v = 9.0, rho = 0.5;
  int i, j, iterations = 25, active_neigh = 1, no_of_bridges = 1, total_neigh = 1;
  bool this_bridge = 1;
  vector<int> sockets_server(total_neigh,0);
  vector<int> sockets_client(total_neigh, 0);
  attributes temp;
  temp.inter_this = "172.16.0.14";
  temp.connect_type = '0';
  vector<attributes> all_neighbors_attr(total_neigh, temp);
  vector<attributes> all_neighbors_attr_bridge;
  vector< vector<int> > bridge_neighbors;
  initialize_all_addresses(all_neighbors_attr);
  all_neighbors_attr_bridge = all_neighbors_attr;
  create_connections(all_neighbors_attr, sockets_server, sockets_client);
  cout<<"\nsockets_server: "<<sockets_server[0]<<"\n";
  cout<<"\nsockets_client: "<<sockets_client[0]<<"\n";
  //receive_bridge_neighbors(all_neighbors_attr,sockets_client,bridge_neighbors);
  //send_bridge_neighbors(all_neighbors_attr, sockets_server, this_bridge);
  no_of_bridges = compute_no_of_bridges(all_neighbors_attr, this_bridge);
  vector<double> u_b(no_of_bridges, 0.0);
  vector<double> z_b(no_of_bridges, 0.0);
  thread send_br_neighbors(send_bridge_neighbors, std::ref(all_neighbors_attr), std::ref(sockets_server), this_bridge);
  thread receive_br_neighbors(receive_bridge_neighbors, std::ref(all_neighbors_attr), std::ref(sockets_client), std::ref(bridge_neighbors));
  send_br_neighbors.join();
  receive_br_neighbors.join();
  admm_z msg_recv_serv;
  msg_recv_serv.z = 0.0;
  vector<admm_z> msg_recv_serv_vec(all_neighbors_attr.size(),msg_recv_serv);
  msg_send_serv.u = 0.0;
  msg_send_serv.x = (2*v + 0.0 - rho*msg_send_serv.u*no_of_bridges)/(2+rho*no_of_bridges); 
  msg_send.z = 0.0;
  double sum_u_b = 0.0, z_value;
  t_1 = clock();
  for (j = 0; j < iterations; j++)
  {
    cout<<"\nhere.\n";
    avg.x = msg_send_serv.x;
    avg.u = msg_send_serv.u;
    z_value = msg_send.z;
    //bridge_computation(total_neigh, sockets_server, avg, msg_send, all_neighbors_attr);
    thread bridge(bridge_node_computation, std::ref(sockets_server), std::ref(avg), std::ref(msg_send), std::ref(all_neighbors_attr_bridge), this_bridge);
    //normal_node_computation(all_neighbors_attr, sockets_client, total_neigh, msg_send, msg_send_serv, sum_u_b, this_bridge, rho, v);
    thread normal(normal_node_computation, std::ref(all_neighbors_attr), std::ref(sockets_client), z_value, std::ref(msg_send_serv), std::ref(msg_recv_serv_vec), std::ref(sum_u_b), this_bridge, rho, v);
    cout<<"after creating normal node thread.\n";
    bridge.join();
    normal.join();
  }
  t_2 = clock();
  float seconds = (float(t_2) - float(t_1))/CLOCKS_PER_SEC;
  cout<<"\nConsensus value: "<<msg_send.z<<"\n";
  cout<<"Value of x: "<<msg_send_serv.x<<"\n";
  cout<<"Value of v: "<<v<<"\n";
  cout<<"seconds: "<<seconds<<"\n";
  return 0;
}

#include <iostream>
#include <fstream>
#include <typeinfo>
#include <math.h>
#include <string.h>
#include <cstring>
#include <sstream>
#include <bitset>
#include <cstdlib>
#include <iomanip>

using namespace std;

string decimal_to_hex(unsigned int );
int a,b,c,d,f,g,h,i,j,k,l,m=0;
int curr_trace,trace_size;

int Time_stamp;

class file_stream
{
 public:
 string access;
 unsigned int hex_addr;
};


class tag
{
  public:
  int valid;
  int dirty;
  int timestamp;
  unsigned int btag;
};

class cache
{
  public:
  int cache_level;
  int assoc;
  int cache_size;
  int block_size;
  int r_policy;
  int incl_policy;
  tag** tag_arr;
  int sets;
  int index_bits;
  int offset_bits; 
  unsigned int t_mask;
  unsigned int i_mask;
  public:
  cache(int blocksize, int level, int size, int association, int rep_policy, int inclusion_policy)
  {
    cache_level=level;
    block_size=blocksize;
    cache_size=size;
    assoc=association;
    r_policy=rep_policy;
    incl_policy=inclusion_policy;
    if (size!=0)
    {
      sets=(size/(blocksize*association));
      offset_bits=log2(block_size);
      index_bits=log2(sets);
      i_mask = (sets-1) << offset_bits;
      t_mask = ((unsigned int)pow(2,(32 - (offset_bits+index_bits))) - 1) << (offset_bits+index_bits);
      tag_arr=new tag*[sets];
      for (int i=0;i<sets;i++)
          tag_arr[i]=new tag[assoc];
    }
  }
  void Read(unsigned int);
  void Write(unsigned int);
  unsigned int getting_tag(unsigned int);
  unsigned int getting_index(unsigned int);
  int allocate(int);
};

file_stream *file_trace;
cache *L1,*L2;
void print(int,int,string);

int main(int argc, char *argv[]) 
{
  int blocksize=atoi(argv[1]);
  int l1_size=atoi(argv[2]);
  int l1_assoc=atoi(argv[3]);
  int l2_size=atoi(argv[4]);
  int l2_assoc=atoi(argv[5]);
  int rep_policy=atoi(argv[6]);
  int inclusion_policy=atoi(argv[7]);
  string trace_file=argv[8];
  

  Time_stamp=0;
  //Initializing Cache
  L1=new cache(blocksize,1,l1_size,l1_assoc,rep_policy,inclusion_policy);
    
  L2=new cache(blocksize,2,l2_size,l2_assoc,rep_policy,inclusion_policy);
  
  //counting number of lines
  fstream filename1;
  filename1.open(argv[8],ios::in);
  int number_of_lines=0;
  if (filename1.is_open()==false)
    cout<<"Error opening file";
  else
  {
    string word1;
    while (!filename1.eof())
    {   
          filename1>>word1;
          number_of_lines++;
    }
  }
  filename1.close();

  file_trace=new file_stream[number_of_lines/2];

  //putting into trace object array
  fstream filename2;
  int i=0;
  filename2.open(argv[8],ios::in);
  if (filename2.is_open()==false)
    cout<<"Error opening file";
  else
  {
    string word2;
    while (!filename2.eof())
    {   
          filename2>>word2;
          file_trace[i].access=word2;
          filename2>>word2;
          string addr=word2;
          if (addr.length()<8)
          {
            int number_of_zeros=8-addr.length();
            addr.insert(0, number_of_zeros, '0');
          }
          unsigned int number=stol(addr,nullptr,16);
          file_trace[i].hex_addr=number;
          i=i+1;
    }
  }
  filename2.close();

  int cnt=1;
  trace_size=number_of_lines/2;
  //reading and writing into cache
  for(curr_trace=0;curr_trace<trace_size;curr_trace++)
  {
      string s=file_trace[curr_trace].access;
      if(s=="r")
        L1->Read(file_trace[curr_trace].hex_addr);
      else if(s=="w")
        L1->Write(file_trace[curr_trace].hex_addr);
      cnt=cnt+1;
  }
  print(rep_policy,inclusion_policy,argv[8]);
  return 0;
}


int cache::allocate(int index)
{
    int way=0; 
    int min_ts=tag_arr[index][0].timestamp;
    //finding invalid or victim block
    for(int i=0;i<assoc;i++)
    {
      if (tag_arr[index][i].valid==0)
      {
          return i;
      }
      if(min_ts>tag_arr[index][i].timestamp)
      {
          min_ts=tag_arr[index][i].timestamp;
          way=i;
      }
    }

    //if replacement policy is Optimal
    if(r_policy==2)
    {
      int farthest_occurrence=0;
      for(int i=0;i<assoc;i++)
      {
        int occurence=0;
        int j=0;
        for(j=curr_trace+1;j<trace_size;j++)
        {
          unsigned int block_tag=getting_tag(file_trace[j].hex_addr);
          int index1=getting_index(file_trace[j].hex_addr);
          if(tag_arr[index][i].btag==block_tag && index==index1)
          {
            occurence=j;
            break;
          }
        }
        if (j==trace_size)
        {
          farthest_occurrence=trace_size;
          way=i;
          break;
        }
        else if(farthest_occurrence<occurence)
        {
          farthest_occurrence=occurence;
          way=i;
        }
      }
    }
    
    //finding victim block address
    unsigned int vaddr=(tag_arr[index][way].btag<<index_bits)+index;
    vaddr=vaddr<<offset_bits;

    if(tag_arr[index][way].dirty==1)
    {
      if(cache_level==1) f++; else l++;
      
      if(L2->cache_size==0||cache_level==2)
        m++; //Writeback to Memory
      else
        L2->Write(vaddr); //writeback victim to next level
    }


    //if inclusive 
    if(incl_policy==1)
    {
      if(cache_level>1)
      {
        int index1 = L1->getting_index(vaddr);
        unsigned int block_tag = L1->getting_tag(vaddr);
        int found=-1;
        //Checking if the victim block exists in L1
        for(int i=0;i<L1->assoc;i++)
        {
          if(L1->tag_arr[index1][i].valid==1 && L1->tag_arr[index1][i].btag==block_tag)
          {
              found=i;
              break;
          } 
        }
           
        if(found!=-1)
        {
          //We have to invalidate the block so first we check if its dirty
          if(L1->tag_arr[index1][found].dirty==1)
            m++;  //WriteBack to Memory
          L1->tag_arr[index1][found].valid=0;
        }
       }
    }

    return way;
}

unsigned int cache::getting_tag(unsigned int addr)
{
    return (addr & t_mask) >> (offset_bits + index_bits) ;
}

unsigned int cache::getting_index(unsigned int addr)
{
    return (addr & i_mask) >> (offset_bits) ;
}

void cache::Read(unsigned int hex_addr)
{
    unsigned int block_tag=getting_tag(hex_addr);
    int index=getting_index(hex_addr);
  
    int blockfound_index=-1;
    if(cache_level==1) //Cache reads
      a++;
    else
      g++;
    
    //Checking if block already exists in cache
    for(int i=0;i<assoc;i++)
    {
      if(tag_arr[index][i].valid==1 && tag_arr[index][i].btag==block_tag)
      {
        blockfound_index=i;
        break; 
      }
    }
    //if block found
    if (blockfound_index!=-1){
        if (r_policy==0)
          tag_arr[index][blockfound_index].timestamp=Time_stamp++;
    }
    //if block not found
    else
    {
        if(cache_level==1) //Cache read misses
          b++; else h++;
        int way=allocate(index);
        if(L2->cache_size!=0 && cache_level==1) //If next level of Cache exists
        {
            L2->Read(hex_addr);
        }
        else 
            m++; 
        tag_arr[index][way].btag=block_tag;
        tag_arr[index][way].valid=1;
        tag_arr[index][way].dirty=0;
        tag_arr[index][way].timestamp=Time_stamp++; 
      }
}

void cache::Write(unsigned int hex_addr)
{
  unsigned int block_tag=getting_tag(hex_addr);
  int index=getting_index(hex_addr);
  int blockfound_index=-1;

  if(cache_level==1) //Cache Writes
    c++;
  else
    i++;

  //Checking if block already exists in cache
  for(int i=0;i<assoc;i++)
  {
    if(tag_arr[index][i].valid==1 && tag_arr[index][i].btag==block_tag)
    {
      blockfound_index=i;
      break; 
    }
  }

  if (blockfound_index!=-1)
  {
    tag_arr[index][blockfound_index].dirty=1;
    tag_arr[index][blockfound_index].timestamp = Time_stamp++;
  }
  else
  {
    if(cache_level==1) //Cache Write misses
      d++; else j++;
    int way=allocate(index);
    if(L2->cache_size!=0 && cache_level==1) //If next level of Cache exists
            L2->Read(hex_addr);
    else 
      m++;
    tag_arr[index][way].btag=block_tag;
    tag_arr[index][way].valid=1;
    tag_arr[index][way].dirty=1;
    tag_arr[index][way].timestamp=Time_stamp++;
  }
}

string decimal_to_hex(unsigned int tag_addr)
{
    string hex_addr="";
    string add1;
    while(tag_addr>0)
    {
      if(tag_addr%16==10)
        add1="a";
      else if(tag_addr%16==11)
        add1="b";
      else if(tag_addr%16==12)
        add1="c";
      else if(tag_addr%16==13)
        add1="d";
      else if(tag_addr%16==14)
        add1="e";
      else if(tag_addr%16==15)
        add1="f";
      else
        add1=to_string(tag_addr%16);
      hex_addr=add1+hex_addr;
      tag_addr=tag_addr/16;
    }
    return hex_addr;
}

void print(int repl_policy,int incl_policy,string trace_file)
{
   cout<<"===== Simulator configuration =====";
   cout<<"\nBLOCKSIZE:             "<<L1->block_size;
   cout<<"\nL1_SIZE:               "<<L1->cache_size;
   cout<<"\nL1_ASSOC:              "<<L1->assoc;
   cout<<"\nL2_SIZE:               "<<L2->cache_size;
   cout<<"\nL2_ASSOC:              "<<L2->assoc;
   if(repl_policy==0)
      cout<<"\nREPLACEMENT POLICY:    LRU";
   else if(repl_policy==1)
      cout<<"\nREPLACEMENT POLICY:    PLRU";
   else if(repl_policy==2)
      cout<<"\nREPLACEMENT POLICY:    Optimal";
   if(incl_policy==0)
      cout<<"\nINCLUSION PROPERTY:    non-inclusive";
   else if(incl_policy==1)
      cout<<"\nINCLUSION PROPERTY:    inclusive";
   cout<<"\ntrace_file:            "<<trace_file;
   cout<<"\n===== L1 contents =====";
   for (int i=0;i<L1->sets;i++)
   {
     cout<<"\nSet   "<<i<<": ";
     for (int j=0;j<L1->assoc;j++)
     {
         string hex_addr;
         if(L1->tag_arr[i][j].dirty==1)
         {
          hex_addr=decimal_to_hex(L1->tag_arr[i][j].btag);
          cout<<hex_addr<<" D  ";
         }
         else
         {
           hex_addr=decimal_to_hex(L1->tag_arr[i][j].btag);
           cout<<hex_addr<<"    ";
         }
     }
   }
   if(L2->cache_size!=0)
   {
     cout<<"\n===== L2 contents =====";
     for (int i=0;i<L2->sets;i++)
     {
      cout<<"\nSet   "<<i<<": ";
      for (int j=0;j<L2->assoc;j++)
      {
          string hex_addr;
          if(L2->tag_arr[i][j].dirty==1)
          {
            hex_addr=decimal_to_hex(L2->tag_arr[i][j].btag);
            cout<<hex_addr<<" D  ";
          }
          else
          {
            hex_addr=decimal_to_hex(L2->tag_arr[i][j].btag);
            cout<<hex_addr<<"    ";
          }
      }
     }
   }
   cout<<"\n===== Simulation results (raw) =====";
   cout<<"\na. number of L1 reads:        "<<a; 
   cout<<"\nb. number of L1 read misses:  "<<b;
   cout<<"\nc. number of L1 writes:       "<<c;
   cout<<"\nd. number of L1 write misses: "<<d;
   float e=(float)(b+d)/(a+c);
   cout << setprecision(6) << fixed;
   cout<<"\ne. L1 miss rate:              "<<e;
   cout<<"\nf. number of L1 writebacks:   "<<f;
   cout<<"\ng. number of L2 reads:        "<<g;
   cout<<"\nh. number of L2 read misses:  "<<h;
   cout<<"\ni. number of L2 writes:       "<<i;
   cout<<"\nj. number of L2 write misses: "<<j;
   if(L2->cache_size==0)
    cout<<"\nk. L2 miss rate:              "<<k;
   else
   {
    float k=(float)(h)/(float)(g);
    cout << setprecision(6) << fixed;
    cout<<"\nk. L2 miss rate:              "<<k;
   }
   cout<<"\nl. number of L2 writebacks:   "<<l;
   if(L2->cache_size==0)
    m=b+d+f;
   else if(L2->cache_size!=0 && incl_policy==0)
    m=h+j+l;
   else if(L2->cache_size!=0 && incl_policy==1)
    m=m;
   cout<<"\nm. total memory traffic:      "<<m;
}
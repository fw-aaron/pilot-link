
#ifndef _PILOT_SOCKADDR_H_
#define _PILOT_SOCKADDR_H_

/* Define if you have the sa_len member of struct sockaddr */
#undef HAVE_SA_LEN

#ifdef HAVE_SA_LEN
struct pi_sockaddr {
  unsigned char pi_len;
  unsigned char pi_family;
  char pi_device[14];
};
#else
struct pi_sockaddr {
  unsigned short pi_family;
  char pi_device[14];
};
#endif

#endif /* _PILOT_SOCKADDR_H_ */
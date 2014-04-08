#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;

  for (i = 0; i < argc; i++)
  	//printf("LINE 10!\n");
    printf ("%s ", argv[i]);
  printf ("\n");

  return EXIT_SUCCESS;
}

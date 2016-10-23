#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_SIZE 100
#define BUFF_SIZE 1024
typedef struct file {

	int fd; // number of file
	ssize_t rd_size; // rd_size
	int i; 	// position of buffer
	char buf[BUFF_SIZE]; // buffer
}file ;

int readaline_and_out(file *fin, int fout); 
void push(char item);
char pop();
char stack[MAX_SIZE];
int top = -1;
int
main(int argc, char *argv[])
{
    int  fout;
	file file1 = {0,0,BUFF_SIZE+1,0},file2 = {0, 0,BUFF_SIZE+1 ,0};
	int eof1 = 0, eof2 = 0;
    long line1 = 0, line2 = 0, lineout = 0;
    struct timeval before, after;
    int duration;
    int ret = 1;
    if (argc != 4) {
        fprintf(stderr, "usage: %s file1 file2 fout\n", argv[0]);
        goto leave0;
    }
	
    if ((file1.fd = open(argv[1], O_RDONLY)) < 0) {
		perror(argv[1]);
        goto leave0;
    }
	
    if ((file2.fd = open(argv[2], O_RDONLY)) < 0) {
        perror(argv[2]);
        goto leave1;
    }
    if ((fout = open(argv[3], O_WRONLY|O_TRUNC|O_CREAT, 0644)) < 0) {
        perror(argv[3]);
        goto leave2;
    }

    gettimeofday(&before, NULL);
	
	/////
	do {
        if (!eof1) {
            if (!readaline_and_out(&file1, fout)) {
                line1++; lineout++;
            } else
                eof1 = 1;
        }
        if (!eof2) {
            if (!readaline_and_out(&file2, fout)) {
                line2++; lineout++;
            } else
                eof2 = 1;
        }
//		sleep(1); // sleep 함수
    } while (!eof1 || !eof2);
	//////
	gettimeofday(&after, NULL);
    
    duration = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);
    printf("Processing time = %d.%06d sec\n", duration / 1000000, duration % 1000000);
    printf("File1 = %ld, File2= %ld, Total = %ld Lines\n", line1, line2, lineout);
    ret = 0;
 

leave3:
    close(fout);
leave2:
    close(file2.fd);
leave1:
    close(file1.fd);

leave0:
    return ret; 

}

/* Read a line from fin and write it to fout */
/* return 1 if fin meets end of file */

int
readaline_and_out(file *fin, int fout)
{  
	
   	int count = fin->i;
	int i = 0, j = 0;
	char buff[MAX_SIZE];	
	int tmp = 0;

	push('\n'); 

	do{
		if(	count >=  BUFF_SIZE ){ // end of buffer??
			fin->rd_size = read(fin->fd, fin->buf, BUFF_SIZE);
			count = 0;
			fin->i = 0; // buffer start position.
		
			if( (fin->rd_size == 0) ){
				top = -1;
				return 1; break;
			}// end of file
		} // buffer setting 
		
			push(fin->buf[count]);
			if ( (fin->buf[count] == 0x0a) ){
				count++;
				fin->i = count;
				top--;
				break; // end of line ??
			}
			count++;
   	}while(1); // end of line ??
	

	while(top+1){
	
		buff[j++] = pop();	
    }
	
	write(fout, buff, j);  // add '\n'    
	
	return 0; //

	}
void push(char item)
{
	stack[++top] = item;
}
char pop()
{
	return stack[top--];
}

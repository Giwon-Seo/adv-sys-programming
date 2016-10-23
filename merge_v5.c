#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define MAX_STACK_SIZE 100
#define BUFF_SIZE 512 

typedef struct file{
	FILE *fd;
	char buf[BUFF_SIZE];
	int rd_size;
	int i;
}file;

int stack[MAX_STACK_SIZE]; 
int top = -1; 
int pop(); 
void push(int item); 

int readaline_and_out(file *fin, file *fout); 

int
main(int argc, char *argv[])
{
    file file1, file2, fout;
    int eof1 = 0, eof2 = 0;
    long line1 = 0, line2 = 0, lineout = 0;
    struct timeval before, after;
    int duration;
    int ret = 1;

    if (argc != 4) {
        fprintf(stderr, "usage: %s file1 file2 fout\n", argv[0]);
        goto leave0;
    }
    if ((file1.fd = fopen(argv[1], "rt")) == NULL) {
        perror(argv[1]);
        goto leave0;
    }
    if ((file2.fd = fopen(argv[2], "rt")) == NULL) {
        perror(argv[2]);
        goto leave1;
    }
    if ((fout.fd = fopen(argv[3], "wt")) == NULL) {
        perror(argv[3]);
        goto leave2;
    }
    
    gettimeofday(&before, NULL);
    do {
        if (!eof1) {
            if (!readaline_and_out(&file1, &fout)) {
                line1++; lineout++;
            } else
                eof1 = 1;
        }
        if (!eof2) {
            if (!readaline_and_out(&file2, &fout)) {
                line2++; lineout++;
            } else
                eof2 = 1;
        }
    } while (!eof1 || !eof2);
    gettimeofday(&after, NULL);
    
    duration = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);
    printf("Processing time = %d.%06d sec\n", duration / 1000000, duration % 1000000);
    printf("File1 = %ld, File2= %ld, Total = %ld Lines\n", line1, line2, lineout);
    ret = 0;
    
leave3:
    fclose(fout.fd);
leave2:
    fclose(file2.fd);
leave1:
    fclose(file1.fd);
leave0:
    return ret; 
}

/* Read a line from fin and write it to fout */
/* return 1 if fin meets end of file */
int
readaline_and_out(file *fin, file *fout)
{    
    int ch, count = fin->i;
	int i = 0 , j = 0 ;
	int tmp = 0;
	char buff[MAX_STACK_SIZE];

	push('\n');

	do{
		if( count >= BUFF_SIZE ) { 
			fin->rd_size = fread(fin->buf,sizeof(char),BUFF_SIZE,fin->fd);
			count = 0;
			fin -> i = 0; // buffer start position.
	
			if( ( fin->rd_size == 0) ){
				top = -1;
				return 1; break;
			}
		}

		push(fin->buf[count]);
		if ( ( fin->buf[count] == 0x0a) ){
			
			count++;
			fin->i =count;
			top--;
			break;
		}
		count++;
	}while(1);

	
	while(top + 1){
		buff[j++] = pop();
	}

	fwrite(buff,sizeof(char),j,fout->fd);

	return 0;
}
void push(int item)
{
	stack[++top] = item;
}
int pop()
{
	return stack[top--];
}

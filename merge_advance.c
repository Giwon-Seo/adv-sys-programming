#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define MyFile_len 104857600 // 100 MB
typedef struct MyFile {

	int fd; // file directory
	char *buf; // file buffer

}MyFile ;
    
long line1 = 0, line2 = 0, lineout = 0;

int readaline_and_out(char *fin1, char *fin2 , char *fout ); 

int
main(int argc, char *argv[])
{
	MyFile rf1, rf2, wf; // read MyFile 1, 2 / write file 1
	int eof1 = 0, eof2 = 0; // end of MyFile
    struct timeval before, after;
    int duration;
    int ret = 1;
	
	int size = 0;

	int flag = PROT_WRITE | PROT_READ;

	// MyFile open
	if (argc != 4) {
        fprintf(stderr, "usage: %s rf1 rf2 wf\n", argv[0]);
        goto leave0;
    }
	
    if ((rf1.fd = open(argv[1], O_RDWR)) < 0) {   // O_RDONLY -> O_RDWR
		perror(argv[1]);
        goto leave0;
    }
	
    if ((rf2.fd = open(argv[2], O_RDWR)) < 0) {  //
        perror(argv[2]);
        goto leave1;
    }
    if (( wf.fd = open(argv[3], O_RDWR|O_CREAT, 0644)) < 0) {
        perror(argv[3]);
        goto leave2;
    }
	
	ftruncate(wf.fd, MyFile_len * 2); // make NUll file  
	
	// MyFile - memory mapping //

	if( (rf1.buf = mmap( 0 , MyFile_len, flag, MAP_SHARED, rf1.fd, 0 ) ) == NULL) {
		perror("mmap error");
		goto leave3;
	}
	if( (rf2.buf = mmap( 0 , MyFile_len, flag, MAP_SHARED, rf2.fd, 0 ) ) == NULL) {
		perror("mmap error");
		goto leave4;
	}	
	if( (wf.buf = mmap( 0 , MyFile_len * 2, flag, MAP_SHARED, wf.fd, 0 ) ) == NULL) {
		perror("mmap error");
		goto leave5;
	}

    gettimeofday( &before, NULL );
	// readaline and merge rf1 with rf2 into wf

	readaline_and_out( rf1.buf , rf2.buf, wf.buf ); 
    
	// Time measurement //
	gettimeofday( &after, NULL );
	duration = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);
    printf("Processing time = %d.%06d sec\n", duration / 1000000, duration % 1000000);
    printf("File1 = %ld, File2= %ld, Total = %ld Lines\n", line1, line2, lineout);
    ret = 0;
 
// file close and munmap//
	munmap(wf.buf, MyFile_len * 2 );
leave5:
	munmap(rf2.buf, MyFile_len);
leave4:
	munmap(rf1.buf, MyFile_len);
leave3:
	close(wf.fd);
leave2:
    close(rf2.fd);
leave1:
	close(rf1.fd);
leave0:
    return ret; 

}

/* Read a line from fin and write it to wf */
/* return 1 if fin meets end of MyFile */

int 
readaline_and_out(char *fin1, char *fin2 , char *fout ) 
{ 
	char *pfile1, *pfile2, *pfileout;	// fin1, fin2, fout 파일의 위치
	int rsize1, rsize2; // file1, file2의 처리해야하는 문자수
	char *hline, *tline, *iline; // fout 파일의 head, tail, index
	int cfile = 1; // 현재 파일 위치
 
	pfile1 = fin1; pfile2 = fin2; pfileout = fout; 
	// 각 파일별 처음 위치 설정
   
    rsize1 = MyFile_len; rsize2 = MyFile_len;
	// 각 파일별 남아있는 문자수 설정

	cfile = 1; // 첫번째 파일 선택
    hline = pfile1; // 처음 hline 지정
    
	while(rsize1 || rsize2){
		switch(cfile){
			case 1:
            rsize1--;
	        if((*pfile1++) == '\n'){
			      line1++; 
				  if(rsize2) cfile = 2;
			      tline = pfile1; 
			      iline = --tline; //현재 위치를 잡고, tail은 '\n'을 가르킴
			      while(1){  //마지막껀 \n이므로 제외해야함
			         *pfileout++ = *(--iline); //index는 '\n'보다 하나 전부터 index
			         if(iline==hline){ //라인의 첫번째 인덱스면
			        	 *pfileout++ = *tline; //'\n'을 넣음
						if(cfile ==2)	  hline = pfile2; 
						else if(cfile == 1 ) hline =pfile1; 
						// 현재 파일의 위치에따라 hline 을 바꿔준다.
						break;
					 }
				  }
			}
			break;
	
			case 2:
              rsize2--;
	          if((*pfile2++) == '\n'){
			      line2++; 
				if(rsize1)	  cfile = 1;
			      tline = pfile2; 
			      iline = --tline; //현재 위치를 잡고, tail은 '\n'을 가르킴
			      while(1){  //마지막껀 \n이므로 제외해야함
			         *pfileout++ = *(--iline); //index는 '\n'보다 하나 전부터 index
			         if(iline == hline){ //라인의 첫번째 인덱스면
			        	 *pfileout++ = *tline; //'\n'을 넣음
						 if(cfile == 1) hline = pfile1; 
						 else if(cfile == 2) hline = pfile2; 
						// 현재 파일의 위치에따라 hline 을 바꿔준다.
						break;
						 break;
					 }
				  }
			 }
			  break;
		} // end of switch
	} // end of while
	lineout = line1 + line2;		
	return 1; // end of MyFile
}

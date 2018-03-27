#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/mman.h>

//structure for header of section file
typedef struct section_file_header
{
    char magic[2];
    unsigned short size;
    unsigned char version;
    unsigned char sect_nr;
}section_file_header;

//structure for header of a separate section
typedef struct section_header
{
    char name[16];
    unsigned int type;
    unsigned int offset;
    unsigned int size;
}section_header;

/* Procedure sends string field on pipe fd*/
void sendString(int fd, char* req){
    unsigned char len = strlen(req);
    write(fd, &len, sizeof(unsigned char));
    write(fd, req, len);
}

/*Procedure sends number field on pipe fd*/
void sendNumber(int fd, unsigned int nr){
    write(fd, &nr, sizeof(unsigned int));
}

/*Procedure reads string field from pipe fd*/
char* getString(int fd){
    unsigned char len;
    char* buf;
    read(fd, &len, 1);
    buf = (char*)malloc((len+1)*sizeof(char));
    read(fd, buf, len);
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv)
{
    //pipes
    int fdResp, fdReq;

    //pointers to the memory locations
    void* pShm;
    void* mappedFile;

    //useful strings
    char* req;
    char* path;

    //shared memory id
    int id;

    //descriptors, value holder variables
    int size;
    int fdFile;
    unsigned int fileSize, sect_nr;
    unsigned int offset, value, nr_bytes;


    //section file data
    struct section_file_header* sf_header = (section_file_header*)malloc(sizeof(section_file_header));
    struct section_header* sect_header = (section_header*)malloc(sizeof(section_header));

    //create response pipe
    if (mkfifo("RESP_PIPE_90443", 0600) < 0) {
        printf("ERROR\ncannot create the response pipe\n");
       exit(1);
    }

    //open request pipe
    fdReq = open("REQ_PIPE_90443", O_RDONLY);
    if (fdReq < 0) {
        printf("ERROR\ncannot open the request pipe\n");
        exit(1);
    }

    //open response pipe (connect to it)
    fdResp = open("RESP_PIPE_90443", O_WRONLY);
    unsigned char len = strlen("CONNECT");
    write(fdResp, &len, sizeof(unsigned char));
    write(fdResp, "CONNECT", strlen("CONNECT"));


    printf("SUCCESS\n");        //yay


    while(1){
        req = getString(fdReq);

        if (strcmp(req, "PING") == 0){
            sendString(fdResp, "PING");
            sendString(fdResp, "PONG");
            sendNumber(fdResp, 90443);
        }
        else if (strcmp(req, "CREATE_SHM") == 0){

            read(fdReq, &size, sizeof(int));
            sendString(fdResp, "CREATE_SHM");

            //creating the shared memory region
            id = shmget(14579, size, IPC_CREAT | 0664);
            if (id < 0){
                sendString(fdResp, "ERROR");
                continue;
            }

            //attaching
            pShm = (void*) shmat(id, 0, 0);
            if (pShm == (void*)-1){
                sendString(fdResp, "ERROR");
                continue;
            }
            sendString(fdResp, "SUCCESS");

        }
        else if (strcmp(req, "WRITE_TO_SHM") == 0){

            read(fdReq, &offset, sizeof(unsigned int));
            read(fdReq, &value, sizeof(unsigned int));
            sendString(fdResp, "WRITE_TO_SHM");

            //verifies if offset valid, if value fits into the region
            if (offset < 0 || (offset + sizeof(unsigned int)) > size){
                sendString(fdResp, "ERROR");
                continue;
            }
            memcpy(pShm + offset, &value, sizeof(unsigned int));
            sendString(fdResp, "SUCCESS");
        }
        else if (strcmp(req, "MAP_FILE") == 0){
            path = getString(fdReq);
            sendString(fdResp, "MAP_FILE");

            //trynna work with da file
            if ((fdFile = open(path, O_RDONLY)) < 0){
                sendString(fdResp, "ERROR");
                continue;
            }

            struct stat filestat;
            if (fstat(fdFile, &filestat) < 0){
                sendString(fdResp, "ERROR");
                continue;
            }

            fileSize = filestat.st_size;

            //mapping file into memory
            mappedFile = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fdFile, 0);
            if (mappedFile == (void*) -1){
                sendString(fdResp, "ERROR");
                continue;
            }

            sendString(fdResp, "SUCCESS");
        }
        else if (strcmp(req, "READ_FROM_FILE_OFFSET") == 0){
            read(fdReq, &offset, sizeof(unsigned int));
            read(fdReq, &nr_bytes, sizeof(unsigned int));
            sendString(fdResp, "READ_FROM_FILE_OFFSET");

            if (offset < 0 || (offset + sizeof(unsigned int)) > fileSize){
                sendString(fdResp, "ERROR");
                continue;
            }

            //we also verify if these things exist
            if (pShm == (void*)-1){
                sendString(fdResp, "ERROR");
                continue;
            }
            if (mappedFile == (void*) -1){
                sendString(fdResp, "ERROR");
                continue;
            }
            memcpy(pShm, mappedFile + offset, nr_bytes);

            sendString(fdResp, "SUCCESS");
        }
        else if (strcmp(req, "READ_FROM_FILE_SECTION") == 0){
            read(fdReq, &sect_nr, sizeof(unsigned int));
            read(fdReq, &offset, sizeof(unsigned int));
            read(fdReq, &nr_bytes, sizeof(unsigned int));
            sendString(fdResp, "READ_FROM_FILE_SECTION");

            if (offset < 0 || (offset + sizeof(unsigned int)) > fileSize) {
                sendString(fdResp, "ERROR");
                continue;
            }

            if (mappedFile == (void*) -1){
                sendString(fdResp, "ERROR");
                continue;
            }

            //reading file header
            memcpy(sf_header, mappedFile, sizeof(section_file_header));

            if (sect_nr < 1 || sect_nr > sf_header->sect_nr){
                sendString(fdResp, "ERROR");
                continue;
            }

            //the numbers represent sizes, could've written sizeof but the line would've been too long like this comment lol
            memcpy(sect_header, mappedFile + 6 + 28*(sect_nr - 1), sizeof(section_header));
            printf("sect name:%s\n", sect_header->name);

            if (pShm == (void*)-1){
                sendString(fdResp, "ERROR");
                continue;
            }

            //we write directly into the shared memory region directly from the memory mapped file
            memcpy(pShm, mappedFile + sect_header->offset + offset, nr_bytes);

            sendString(fdResp, "SUCCESS");
        }
        else if (strcmp(req, "READ_FROM_LOGICAL_SPACE_OFFSET") == 0){
            read(fdReq, &offset, sizeof(unsigned int));
            read(fdReq, &nr_bytes, sizeof(unsigned int));
            sendString(fdResp, "READ_FROM_LOGICAL_SPACE_OFFSET");

            if (pShm == (void*)-1){
                sendString(fdResp, "ERROR");
                continue;
            }
            if (mappedFile == (void*) -1){
                sendString(fdResp, "ERROR");
                continue;
            }

            memcpy(sf_header, mappedFile, sizeof(section_file_header));
            int div, mod;

            //calculating where to read from
            for(int i=0; i<sf_header->sect_nr; i++){
                memcpy(sect_header, mappedFile + 6 + 28*i, sizeof(section_header));
                if (offset < sect_header->size){
                    if (nr_bytes < sect_header->size - offset){
                        memcpy(pShm, mappedFile + sect_header->offset + offset, nr_bytes);
                        break;
                    }
                    else{
                        //case when nr of bytes to be read is above the remaining bytes in the section
                        sendString(fdResp, "ERROR");
                        break;
                    }
                }
                div = sect_header->size/1024;
                mod = sect_header->size%1024;
                //decrease given logical offset by 1024k sections
                if (mod > 0){
                    offset -= 1024*(div+1);
                    continue;
                }
                else
                    offset -= 1024*div;

            }

            sendString(fdResp, "SUCCESS");
        }
        else if (strcmp(req, "EXIT") == 0){
            //cleanup
            close(fdResp);
            close(fdReq);
            exit(1);
        }
        else{
            printf("command unknown\n");
            exit(1);
        }

    }
    free(req);
    free(sf_header);
    free(sect_header);
    free(path);

    shmctl(id, IPC_RMID, 0);

    return 0;
}


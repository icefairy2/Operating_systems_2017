#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h> 
#include <fcntl.h>
#include <unistd.h>

#define MAX 1024

//structure for header of section file
struct section_file_header
{
    char magic[3];
    unsigned short size;
    unsigned char version;
    unsigned char sect_nr;
};

//structure for header of a separate section
struct section_header
{
    char name[17];
    unsigned int type;
    unsigned int offset;
    unsigned int size;
};

/********************************************
 *              HELPER FUNCTIONS
 ********************************************/

 /* Parsing one command line argument of format "param=arg_str"
  * where param is a string given as a parameter to this procedure
  * Procedure returns only the string after the '='
  */
char* get_arg_value(char* arg, char* param)
{
    const char separator[2] = "=";
    char *token;

    /* get the first token */
    token = strtok(arg, separator);

    if (strcmp(token, param) == 0)
    {
        token = strtok(NULL, separator);
        return token;
    }

    return "";
}

/* Procedure to verify if string str starts with string pre
 */
int startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
        lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
}

/* Procedure to read section file header
 * returns -1 in case reading from inside the file fails
 */
int getSection_file_header(int fd, struct section_file_header *sf_header)
{
    //position cursor to beginning of file
    lseek(fd, 0, SEEK_SET);
    int bytesRead;

    //reading magic
    if ((bytesRead = read(fd, &sf_header->magic, 2)) < 0) 
    {
        return -1;
    }
    else
        sf_header->magic[2] = '\0';

    //reading header size
    if ((bytesRead = read(fd, &sf_header->size, 2)) < 0) 
    {
        return -1;
    }

    //reading version
    if ((bytesRead = read(fd, &sf_header->version, 1)) < 0) 
    {
        return -1;
    }

    //reading number of sections
    if ((bytesRead = read(fd, &sf_header->sect_nr, 1)) < 0) 
    {
        return -1;
    }

    return 0;
}

/* Procedure to read i-th section header
* returns -1 in case reading from inside the file fails
*/
int getSection_header(int fd, int i, struct section_header *sect_header)
{
    //position cursor to beginning of section header
    lseek(fd, 6 + 28 * i, SEEK_SET);
    int bytesRead;

    //reading name
    if ((bytesRead = read(fd, &sect_header->name, 16)) < 0) 
    {
        return -1;
    }
    sect_header->name[16] = '\0';

    //reading section type
    if ((bytesRead = read(fd, &sect_header->type, 4)) < 0) 
    {
        return -1;
    }

    //reading offset
    if ((bytesRead = read(fd, &sect_header->offset, 4)) < 0) 
    {
        return -1;
    }

    //reading size
    if ((bytesRead = read(fd, &sect_header->size, 4)) < 0) 
    {
        return -1;
    }

    return 0;
}

/* Procedure to print out the information from the headers
 * used when the parse function was called
 */
void printSection_file(struct section_file_header sf_header, struct section_header sect_header[])
{
    int i;
    printf("version=%hhu\n", sf_header.version);
    printf("nr_sections=%hhu\n", sf_header.sect_nr);
    for (i = 0; i < sf_header.sect_nr; i++)
    {
        printf("section%d: ", i + 1);
        printf("%s %d %d\n", sect_header[i].name, sect_header[i].type, sect_header[i].size);
    }
}

/********************************************
*              COMMAND FUNCTIONS
********************************************/

/* Procedure for listing content
 * dirpath: the path of the directory
 * recursive: 1 if recursive call was made, 0 otherwise
 * size_filter: nr of bytes to which in our requirement the file size should be GREATER
 * name_filter: string with which in our requirement the file name should START WITH
 */
void list_dir(char* dirpath, int recursive, int size_filter, char* name_filter)
{
    DIR *d;
    struct dirent *dir;
    struct stat fileStat;
    char completepath[100];

    d = opendir(dirpath);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".") != 0)
            {
                strcpy(completepath, dirpath);
                strcat(completepath, "/");
                strcat(completepath, dir->d_name);

                if (stat(completepath, &fileStat) == 0)
                    if (fileStat.st_size >= size_filter)
                    {
                        if (startsWith(name_filter, dir->d_name))
                        {
                            printf("%s\n", completepath);
                        }

                    }
                if (recursive)
                {
                    list_dir(completepath, recursive, size_filter, name_filter);
                }
            }
        }
        closedir(d);
    }
}

/* Procedure for verifying if given file complies with the section file format
 * file: the path of the file to be parsed
 */
void parse_file(char* file)
{
    int fd;
    struct section_file_header sf_header;
    struct section_header sect_header[100];
    int i;

    if ((fd = open(file, O_RDONLY)) < 0)
    {
        printf("ERROR\ninvalid file path\n");
        exit(1);
    }

    //read section file header then check if it is correct
    if (getSection_file_header(fd, &sf_header) < 0)
    {
        printf("ERROR\nreading section file header failed\n");
        exit(1);
    }

    if (strcmp(sf_header.magic, "bw") != 0)
    {
        printf("ERROR\nwrong magic\n");
        exit(1);
    }

    if (sf_header.version < 74 || sf_header.version > 100)
    {
        printf("ERROR\nwrong version\n");
        exit(1);
    }

    if (sf_header.sect_nr < 5 || sf_header.sect_nr > 20)
    {
        printf("ERROR\nwrong sect_nr\n");
        exit(1);
    }

    //read section headers and check if their type is correct
    for (i = 0; i < sf_header.sect_nr; i++)
    {
        if (getSection_header(fd, i, &sect_header[i]) < 0)
        {
            printf("ERROR\nreading section file header failed\n");
            exit(1);
        }

        if (sect_header[i].type != 61 && sect_header[i].type != 95 && sect_header[i].type != 25)
        {
            printf("ERROR\nwrong sect_types\n");
            exit(1);
        }
    }

    printf("SUCCESS\n");
    printSection_file(sf_header, sect_header);
}

/* Procedure for extracting line with nr 'line' from section nr 'section'
* file: the path of the file from which we read the line
* section: the index of the section
* line: index of the line to be read (lines are numbered from the last)
*/
void extract_line(char* file, int section, int line)
{
    int fd;
    struct section_header sect_header;
    int i = 0, byteRead;
    char c;

    if ((fd = open(file, O_RDONLY)) < 0)
    {
        printf("ERROR\ninvalid file\n");
        exit(1);
    }

    if (getSection_header(fd, section - 1, &sect_header) < 0)
    {
        printf("ERROR\ninvalid section\n");
        exit(1);
    }

    //printf("%s %d %x %d\n", sect_header.name, sect_header.type, sect_header.offset, sect_header.size);

    lseek(fd, sect_header.offset + sect_header.size + 1, SEEK_SET);

    printf("SUCCESS\n");

    while (i < line) {

        lseek(fd, -2, SEEK_CUR);

        if ((byteRead = read(fd, &c, 1)) < 0) {
            printf("ERROR\ninvalid line\n");
            exit(1);
        }

        if (i == line - 1)
            printf("%c", c);

        if (c == 0x0A) {
            i++;
        }
    }
}

/* Procedure for finding and listing all the section file paths 
* which have at least 4 sections with exactly 13 lines
* dirpath: the path of the directory
*/
void find_all(char* dirpath) {
    DIR *d;
    struct dirent *dir;
    char completepath[100];

    int fd;
    struct section_file_header sf_header;
    struct section_header sect_header;
    int i, j;

    int valid, count, countLines, byteRead;
    char c;

    d = opendir(dirpath);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            //ignoring current and parent directory
            if (strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".") != 0)
            {
                valid = 1;

                strcpy(completepath, dirpath);
                strcat(completepath, "/");
                strcat(completepath, dir->d_name);

                if ((fd = open(completepath, O_RDONLY)) < 0)
                {
                    valid = 0;
                }
                
                //read section file header then check if it is correct
                if (valid && getSection_file_header(fd, &sf_header) < 0)
                {
                    valid = 0;
                }

                if (valid && strcmp(sf_header.magic, "bw") != 0)
                {
                    valid = 0;
                }
               // printf("%s\n", sf_header.magic);

                if (valid && (sf_header.version < 74 || sf_header.version > 100))
                {
                    valid = 0;
                }

                if (valid && (sf_header.sect_nr < 5 || sf_header.sect_nr > 20))
                {
                    valid = 0;
                }

                count = 0;
                //printf("-------\n");
                //read section headers and check if their type is correct
                if (valid && sf_header.sect_nr >= 4) 
                {    
                   // printf("********\n");
                    for (i = 0; i < sf_header.sect_nr; i++)
                    {
                        //read ith section header, exit if invalid
                        if (getSection_header(fd, i, &sect_header) < 0)
                            break;

                        if (sect_header.type != 61 && sect_header.type != 95 && sect_header.type != 25)
                            break;

                        countLines = 1;
                        lseek(fd, sect_header.offset +1, SEEK_SET);

                        for (j = 0; j < sect_header.size; j++) 
                        {
                            if ((byteRead = read(fd, &c, 1)) < 0)
                                break;

                            if (c == 0x0A) {
                                countLines++;
                            }
                        }
                        //printf("%d\n", countLines);
                        if (countLines == 13) 
                        {
                            count++;
                        }
                    }
                }

                if (count >= 4)
                {
                    printf("%s\n", completepath);
                }

                find_all(completepath);

            }
        }
        closedir(d);
    }
}

int main(int argc, char **argv)
{
    //variables used for listing
    DIR* d;
    char aux[100];
    int i;
    char* dirpath = "";
    int recursive = 0;
    int size_filter = 0;
    char* filt_crit = "";

    //variable for parsing
    char* filepath = "";

    //variables for extracting
    int section, line;

    if (argc >= 2)
    {
        if (strstr(argv[1], "variant"))
        {
            printf("90443\n");
        }
        else if (strstr(argv[1], "list"))
        {

            //Check if valid directory path was entered
            dirpath = get_arg_value(argv[argc - 1], "path");
            d = opendir(dirpath);
            if (d)
                printf("SUCCESS\n");
            else
            {
                printf("ERROR\ninvalid directory path\n");
                exit(1);
            }
            closedir(d);

            // check all other arguments
            if (argc >= 3)
            {
                i = 2;
                do
                {
                    if (strcmp(argv[i], "recursive") == 0)
                    {
                        recursive = 1;
                    }
                    else
                    {
                        strcpy(aux, argv[i]);
                        filt_crit = get_arg_value(aux, "size_greater");
                        if (strcmp(filt_crit, "") != 0)
                        {
                            size_filter = atoi(filt_crit);
                            strcpy(aux, "");
                            filt_crit = aux;
                        }
                        else
                        {
                            strcpy(aux, argv[i]);
                            filt_crit = get_arg_value(aux, "name_starts_with");
                        }
                    }
                    i++;
                } while (i < argc - 1);
            }

            //call listing function
            list_dir(dirpath, recursive, size_filter, filt_crit);
        }
        else if (strstr(argv[1], "parse"))
        {
            filepath = get_arg_value(argv[argc - 1], "path");
            parse_file(filepath);
        }
        else if (strstr(argv[1], "extract"))
        {
            //reading all arguments no matter their order
            if (argc == 5)
            {
                i = 2;
                do
                {
                    strcpy(aux, argv[i]);
                    if (strstr(aux, "path"))
                    {
                        filepath = get_arg_value(argv[i], "path");
                        strcpy(aux, "");
                        filt_crit = aux;
                    }
                    else
                    {
                        strcpy(aux, argv[i]);
                        filt_crit = get_arg_value(aux, "section");
                        if (strcmp(filt_crit, "") != 0)
                        {
                            section = atoi(filt_crit);
                            strcpy(aux, "");
                            filt_crit = aux;
                        }
                        else
                        {
                            strcpy(aux, argv[i]);
                            filt_crit = get_arg_value(aux, "line");
                            if (strcmp(filt_crit, "") != 0)
                            {
                                line = atoi(filt_crit);
                                strcpy(aux, "");
                                filt_crit = aux;
                            }
                        }
                    }
                    i++;
                } while (i < argc);

                extract_line(filepath, section, line);
            }
            else
            {
                printf("ERROR\ntoo few arguments\n");
                exit(1);
            }
        }
        else if (strstr(argv[1], "findall"))
        {
            //Check if valid directory path was entered
            dirpath = get_arg_value(argv[argc - 1], "path");
            d = opendir(dirpath);
            if(d)
                printf("SUCCESS\n");
            else
            {
                printf("ERROR\ninvalid directory path\n");
                exit(1);
            }
            closedir(d);

            find_all(dirpath);
        }
    }
    return 0;
}


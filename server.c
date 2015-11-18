#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#define Maxlenline 15000
#define CommandLen 256
#define MaxPipenum 1000
#define Port 7123
#define ClientNum 10

typedef struct superpipe{
	int count;
	int valid;
	int pipe_fd[2];
}superpipe;

int linelen(int fd,char *ptr,int maxlen);
int main(int argc,char *argv[])
{
	int port=Port;
	if (argc==2){
		port=atoi(argv[1]);
	}
	int sockfd,clientfd;
	int *status=0;
	struct sockaddr_in dest;
	char Hello[200] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
	char msg[Maxlenline];
	char buf[Maxlenline];
	char environpath[10][Maxlenline];//max 10 paths
	/*initial*/
	bzero((char *)&dest,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(port);
	dest.sin_addr.s_addr = INADDR_ANY;
	
	int environ_num=0;
	clearenv();
	chdir("/net/gcs/104/0456115/ras");
	setenv("PATH","bin:.",1);//set environment
	strcpy(environpath[environ_num],"PATH");
	environ_num++;
	
	sockfd = socket(PF_INET,SOCK_STREAM,0);//create socket
	if(sockfd < 0){//socket
		fprintf(stderr,"socket error\n");
		exit(1);
	}
	
	bind(sockfd,(struct sockaddr*)&dest,sizeof(dest));
	listen(sockfd,ClientNum);//listen
	printf("SERVER_PORT: %d\n",port);
	
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
	char finish[30];

	for(;;){
		clientfd = accept(sockfd,(struct sockaddr*)&client_addr,&addrlen);
		/*if(clientfd == -1){
			fprintf(stderr,"connect error\n");
			exit(1);
		}*/
		
		int childpid;
		childpid=fork();
		if(childpid == -1) fprintf(stderr,"fork error\n");
		else if(childpid == 0){//child process
			superpipe super_pipe[1000];
			//int num_super_pipe=0;
			for(int i=0;i<1000;i++){
				super_pipe[i].count=0;
				super_pipe[i].valid=0;
			}
			
			char *postfix = "% ";
			close(sockfd);
			
			dup2(clientfd,fileno(stdout));
			dup2(clientfd,fileno(stderr));
			
			write(clientfd,Hello,strlen(Hello));
			write(clientfd,postfix,strlen(postfix));
			
			
			while(linelen(clientfd,buf,sizeof(buf))!=0){
				char commands[Maxlenline];
				char commands2[Maxlenline];
				strcpy(commands,buf);
				
				
				if(strchr(commands,'/')!=0){//To find '/' 
					strcpy(msg,"The character '/' is not be allowed\n");
					write(clientfd,msg,strlen(msg));
				}
				else if(strstr(commands,"exit")!=0){//finished
					close(clientfd);
					return 0;
				}
				else if(strstr(commands,"setenv")!=0){
					for(int i=0;i<1000;i++){
						if(super_pipe[i].count > 0 && super_pipe[i].valid==1)super_pipe[i].count--;
					}
					char *str;
					char pathname[CommandLen];
					char path[CommandLen];
					int exist=0;
					str=strtok(commands," \r\n");//setenv
					str=strtok(NULL," \r\n");//pathname
					if(str==NULL){
						strcpy(msg,"You have to input setenv pathname path\n");
						write(clientfd,msg,strlen(msg));
					}
					else{
						strcpy(pathname,str);
						str=strtok(NULL," \r\n");//path
						if(str==NULL){
							strcpy(msg,"You have to input setenv pathname path\n");
							write(clientfd,msg,strlen(msg));
						}
						else{
							strcpy(path,str);
							setenv(pathname,path,1);
							for(int i=0;i<10;i++){//path exist or not
								if(strcmp(environpath[i],pathname)==0){
									exist=1;
									break;
								}
							}
							if(exist==0){
								strcpy(environpath[environ_num],pathname);
								environ_num++;
							}
						}
					}	
				}
				else if(strstr(commands,"printenv")!=0){
					for(int i=0;i<1000;i++){
						if(super_pipe[i].count > 0 && super_pipe[i].valid==1)super_pipe[i].count--;
					}
					char *str;
					char pathname[CommandLen];
					char path[CommandLen];
					str=strtok(commands," \r\n");//printenv
					str=strtok(NULL," \r\n");//pathname
					if(str==NULL){
						strcpy(msg,"You have to input printenv pathname\n");
						write(clientfd,msg,strlen(msg));
					}
					else{
						strcpy(pathname,str);
						if(getenv(pathname)!=0){
							strcpy(path,pathname);
							strcat(path,"=");
							strcat(path,getenv(pathname));
							strcat(path,"\n");
							write(clientfd,path,strlen(path));
						}
						else{
							strcpy(path,"No such Path\n");
							write(clientfd,path,strlen(path));
						}
					}
				}
				else if(strchr(commands,'|')==NULL && strchr(commands,'!')==NULL){//none pipe
					char *str;
					char pathtmp[CommandLen];
					char path[CommandLen];
					strcpy(pathtmp,getenv("PATH"));//get path
					str=strtok(pathtmp,":");
					strcpy(path,str);
					strcat(path,"/");//   bin/
					str=strtok(commands," \t\n\r");//get command
					if(str != NULL){
						strcat(path,str);//set bin/command
						char *parameters[CommandLen];
						int num_parameter=0;
						parameters[num_parameter]=str;//command
						num_parameter++;
						if(access(path,F_OK)==0){//command exist or not  yes=0 no=-1
							if((str=strtok(NULL," \t\n\r"))!=NULL){
								int filewrite=0;
								char filename[CommandLen];
								if(strcmp(str,">")==0){
									filewrite=1;
								}
								else{
									parameters[num_parameter]=str;
									num_parameter++;
								}
								while((str=strtok(NULL," \t\n\r"))!= NULL){//first parameter
										if(strcmp(str,">")==0){
											filewrite=1;
										}
										else{
											if(filewrite==1){
												strcpy(filename,str);
											}
											else{
												strcpy(filename,str);
												parameters[num_parameter]=str;
												num_parameter++;
											}
										}
								}
								parameters[num_parameter]=(char *)NULL;
								
								int countequal0=-1;
								for(int i=0;i<1000;i++){
									if(super_pipe[i].count > 0 && super_pipe[i].valid ==1)super_pipe[i].count--;
								}
								for(int i=0;i<1000;i++){
									if(super_pipe[i].count==0 && super_pipe[i].valid ==1){//get the number of pipe
										countequal0=i;
										break;
									}
								}
								
								int cmd_pid;
								cmd_pid=fork();
								
								if(cmd_pid==-1){
									char errmsg[CommandLen]="fork error";
									write(clientfd,errmsg,strlen(errmsg));
								}
								else if(cmd_pid==0){//child process
									if(filewrite==1){
										freopen(filename,"w",stdout);
									}
									else{
										dup2(clientfd,fileno(stdout));
									}
									if(countequal0>=0){
										dup2(super_pipe[countequal0].pipe_fd[0],fileno(stdin));//++
										close(super_pipe[countequal0].pipe_fd[1]);
									}
									execv(path,parameters);
									exit(1);
								}
								else{//parent process
									if(countequal0 >=0){
										close(super_pipe[countequal0].pipe_fd[1]);
									}
									wait(status);
								}
								if(countequal0 >=0){
									super_pipe[countequal0].valid=0;
									close(super_pipe[countequal0].pipe_fd[0]);
								}
							}
							else{//execution with no parameters
								
								int countequal0= -1;
								for(int i=0;i<1000;i++){
									if(super_pipe[i].count > 0 && super_pipe[i].valid ==1)super_pipe[i].count--;
								}
								for(int i=0;i<1000;i++){
									if(super_pipe[i].count==0 && super_pipe[i].valid ==1){//get the number of pipe
										countequal0=i;
										break;
									}
								}
								
								int cmd_pid;
								cmd_pid=fork();
								if(cmd_pid==-1){
									char errmsg[CommandLen]="fork error";
									write(clientfd,errmsg,strlen(errmsg));
								}
								else if(cmd_pid==0){
									parameters[num_parameter]=(char *)NULL;
									if(countequal0>=0){
										dup2(super_pipe[countequal0].pipe_fd[0],fileno(stdin));//++
										close(super_pipe[countequal0].pipe_fd[1]);
									}
									dup2(clientfd,fileno(stdout));
									execv(path,parameters);
									exit(1);
								}
								else{
									if(countequal0 >=0){
										close(super_pipe[countequal0].pipe_fd[1]);
									}
									wait(status);
								}
								if(countequal0 >=0){
									super_pipe[countequal0].valid=0;
									close(super_pipe[countequal0].pipe_fd[0]);
								}
							}
						}
						else{//error cmd
							char cmderrmsg[CommandLen+30]="Unknown Command: [";
							strcat(cmderrmsg,str);
							strcat(cmderrmsg,"].");
							strcat(cmderrmsg,"\n");
							write(clientfd,cmderrmsg,strlen(cmderrmsg));
						}
					}
				}
				else{//pipe
					int cmd_num=0;
					char *str;
					char cmds[2500][256];//everyline length not exceed 15000 character
					char temp[Maxlenline];
					strcpy(temp,commands);
					str=strtok(commands,"|!");
					strcpy(cmds[0],str);
					cmd_num++;
					while((str=strtok(NULL,"|!"))!=0){//parse all input
						strcpy(cmds[cmd_num],str);
						cmd_num++;
					}
					
					int ispipe_stderr=0;
					int already_err=0;
					int pipe_err[2];
					int two=0;
					if((str=strchr(temp,'!'))!=NULL){
						ispipe_stderr=1;
						pipe(pipe_err);
					}
					
					int countequal0=-1;
					for(int i=0;i<1000;i++){
						if(super_pipe[i].count > 0 && super_pipe[i].valid ==1)super_pipe[i].count--;
					}
					for(int i=0;i<1000;i++){
						if(super_pipe[i].count==0 && super_pipe[i].valid ==1){//get the number of pipe
							countequal0=i;
							close(super_pipe[i].pipe_fd[1]);
							break;
						}
					}
					int pipe_fd[2];
					int current_read;
					int current_read_temp;
					
					for(int i=0;i<cmd_num;i++){
						char *cmd;
						char path[CommandLen];
						char pathtmp[CommandLen];
						char *parameters[CommandLen];
						int num_parameter=0;
						
						int filewrite=0;
						char filename[CommandLen];
						
						strcpy(pathtmp,getenv("PATH"));//get path
						cmd=strtok(pathtmp,":");
						strcpy(path,cmd);
						strcat(path,"/");//   bin/
						cmd = strtok(cmds[i]," \t\n\r");//get cmd
						strcat(path,cmd);//get bin/command
						parameters[num_parameter]=cmd;//get parameter
						num_parameter++;
						
						if(cmd != NULL){
							if(access(path,F_OK)==-1 && isdigit(*cmd)==0){//error cmd
								for(int i=0;i<1000;i++){
									if(super_pipe[i].count > 0 && super_pipe[i].valid==1)super_pipe[i].count++;
								}
								if(ispipe_stderr==1){
									close(pipe_err[0]);
									close(pipe_err[1]);
								}
								if(i>0){
									close(current_read_temp);
								}
								char cmderrmsg[CommandLen+30]="Unknown Command: [";
								strcat(cmderrmsg,cmd);
								strcat(cmderrmsg,"].");
								strcat(cmderrmsg,"\n");
								write(clientfd,cmderrmsg,strlen(cmderrmsg));
								break;
							}
							else if(access(path,F_OK)==0){//command exist or not  yes=0 no=-1  //bin/n or bin/cmd
								if(i<cmd_num-1)pipe(pipe_fd);
								if((cmd=strtok(NULL," \t\n\r"))!=NULL){
									if(strcmp(cmd,">")==0){
										filewrite=1;
									}
									else{
										parameters[num_parameter]=cmd;
										num_parameter++;
									}
									while((cmd = strtok(NULL, " \t\n\r")) != NULL){
										if(strcmp(cmd,">")==0){
											filewrite=1;
										}
										else{
											if(filewrite==1){
												strcpy(filename,cmd);
											}
											else{
												strcpy(filename,cmd);
												parameters[num_parameter]=cmd;
												num_parameter++;
											}
										}
									}
									parameters[num_parameter]=(char *)NULL;
									
									int cmd_pid;
									cmd_pid=fork();
									if(cmd_pid==-1){
										char errmsg[CommandLen]="fork error";
										write(clientfd,errmsg,strlen(errmsg));
									}
									else if(cmd_pid==0){
										if(filewrite==1 && i==cmd_num-1){
											dup2(current_read,fileno(stdin));
											freopen(filename,"w",stdout);
										}
										else{
											if(ispipe_stderr==1){
												if(cmd_num==2){//removetag0 test.html !1
													dup2(pipe_err[1],fileno(stderr));
													dup2(clientfd,fileno(stdout));
													close(pipe_err[0]);
												}
												else if(cmd_num==3){//removetag0 test.html !1!1
													dup2(pipe_err[1],fileno(stderr));
													dup2(pipe_fd[1],fileno(stdout));
													close(pipe_err[0]);
												}
											}
											else{
												dup2(pipe_fd[1],fileno(stdout));//cat test.html
											}
											close(pipe_fd[0]);
										}
										execv(path,parameters);
										exit(1);
									}
									else{
										close(pipe_fd[1]);
										current_read=pipe_fd[0];
										wait(status);
									}
								}
								else{//execution with no parameters
									parameters[num_parameter]=(char *)NULL;
									int cmd_pid;
									cmd_pid=fork();
									if(cmd_pid==-1){
										char errmsg[CommandLen]="fork error";
										write(clientfd,errmsg,strlen(errmsg));
									}
									else if(cmd_pid==0){
										if(i < cmd_num-1){
											if(i!=0){
												if(ispipe_stderr==0){
													dup2(current_read,fileno(stdin));
													dup2(pipe_fd[1],fileno(stdout));
													close(pipe_fd[0]);
												}
												else{
													if(i == cmd_num-3 ){//cat test.html | number | number !2|2
														dup2(pipe_fd[1],fileno(stdout));
														dup2(pipe_err[1],fileno(stderr));
														close(pipe_err[0]);
														close(pipe_fd[0]);
													}
													else if(i == cmd_num-2){//cat test.html | number | number !2
														dup2(clientfd,fileno(stdout));
														dup2(pipe_err[1],fileno(stderr));
														close(pipe_err[0]);
													}
													else{
														dup2(current_read,fileno(stdin));
														dup2(pipe_fd[1],fileno(stdout));
														close(pipe_fd[0]);
													}
												}
											}
											else{//first command
												if(countequal0>=0){
													dup2(super_pipe[countequal0].pipe_fd[0],fileno(stdin));//++
												}
												dup2(pipe_fd[1],fileno(stdout));
												close(pipe_fd[0]);
											}
										}
										else{//final command
											dup2(current_read,fileno(stdin));
											dup2(clientfd, fileno(stdout));
										}
										execv(path,parameters);
										exit(1);
									}
									else{
										if(i < cmd_num-1){
											close(pipe_fd[1]);
											current_read=pipe_fd[0];
										}
										if(countequal0 >=0){
											close(super_pipe[countequal0].pipe_fd[0]);
											super_pipe[countequal0].valid=0;
											countequal0=-1;//initial
										}
										wait(status);
										if(i>0)
											close(current_read_temp);
										if(i<cmd_num-1)
											current_read_temp=current_read;
									}
								}
							}
							else if(isdigit(*cmd)!=0){// !N |N
								int num;
								num=atoi(cmd);
								char temp_pipe[30000];//new
								char temp_pipe2[30000];//old
								for(int j=0;j<30000;j++){
									temp_pipe[j]='\0';
									temp_pipe2[j]='\0';
								}
								if(ispipe_stderr==1 && already_err==0){
									if(i==cmd_num-2 || i==1){
										str=strtok(str," |!");//!
									}
									if(num==atoi(str)){//correct
										int already=0;
										already_err==1;
										for(int j=0;j<1000;j++){//find the same count
											if(num==super_pipe[j].count && super_pipe[j].valid==1){
												close(pipe_err[1]);
												read(pipe_err[0],temp_pipe,sizeof(temp_pipe)*30000);
												read(super_pipe[j].pipe_fd[0],temp_pipe2,sizeof(temp_pipe2)*30000);
												strcat(temp_pipe2,temp_pipe);
												write(super_pipe[j].pipe_fd[1],temp_pipe2,strlen(temp_pipe2));
												already=1;
												close(pipe_err[0]);
												break;
											}
										}
										for(int j=0;j<1000;j++){
											if(already==1)break;
											if(super_pipe[j].valid==0){
												close(pipe_err[1]);
												super_pipe[j].count=num;
												super_pipe[j].valid=1;
												pipe(super_pipe[j].pipe_fd);
												read(pipe_err[0],temp_pipe,sizeof(temp_pipe)*30000);
												write(super_pipe[j].pipe_fd[1],temp_pipe,strlen(temp_pipe));
												close(pipe_err[0]);
												break;
											}
										}
									}
									else{//|
										int already=0;
										for(int j=0;j<1000;j++){//find the same count
											if(num==super_pipe[j].count && super_pipe[j].valid==1){
												read(current_read,temp_pipe,sizeof(temp_pipe)*30000);
												close(current_read);
												read(super_pipe[j].pipe_fd[0],temp_pipe2,sizeof(temp_pipe2)*30000);
												strcat(temp_pipe2,temp_pipe);
												write(super_pipe[j].pipe_fd[1],temp_pipe2,strlen(temp_pipe2));
												already=1;
												break;
											}
										}
										for(int j=0;j<1000;j++){
											if(already==1)break;
											if(super_pipe[j].valid==0){
												super_pipe[j].count=num;
												super_pipe[j].valid=1;
												pipe(super_pipe[j].pipe_fd);
												read(current_read,temp_pipe,sizeof(temp_pipe)*30000);
												close(current_read);
												write(super_pipe[j].pipe_fd[1],temp_pipe,strlen(temp_pipe));
												break;
											}
										}
									}
								}
								else{//only |
									int already=0;
									for(int j=0;j<1000;j++){//find the same count
										if(num==super_pipe[j].count && super_pipe[j].valid==1){
											read(current_read,temp_pipe,sizeof(temp_pipe)*30000);
											close(current_read);
											read(super_pipe[j].pipe_fd[0],temp_pipe2,sizeof(temp_pipe2)*30000);
											strcat(temp_pipe2,temp_pipe);
											write(super_pipe[j].pipe_fd[1],temp_pipe2,strlen(temp_pipe2));
											already=1;
											break;
										}
									}
									for(int j=0;j<1000;j++){
										if(already==1)break;
										if(super_pipe[j].valid==0){
											super_pipe[j].count=num;
											super_pipe[j].valid=1;
											pipe(super_pipe[j].pipe_fd);
											read(current_read,temp_pipe,sizeof(temp_pipe)*30000);
											close(current_read);
											write(super_pipe[j].pipe_fd[1],temp_pipe,strlen(temp_pipe));
											break;
										}
									}
								}
							}
						}
					}
				}
				write(clientfd,postfix,strlen(postfix));
			}
		}
		else{
			close(clientfd);
			continue;
		}
	}
	return 0;
}
int linelen(int fd,char *ptr,int maxlen)
{
	int n,rc;
	char c;
	for(n=1;n<maxlen;n++){
		if((rc=read(fd,&c,1)) == 1){
			*ptr++ = c;	
			if(c=='\n')  break;
		}
		else if(rc==0){
			if(n==1)     return(0);
			else         break;
		}
		else
			return(-1);
	}
	*ptr=0;
	return(n);
}     

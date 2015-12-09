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
#define ClientNum 30
#define MaxPATH 10
#define MaxPathLen 50
typedef struct superpipe{
	int count;
	int valid;
	int pipe_fd[2];
}superpipe;

typedef struct{
	int valid;
	int exist;
	int pipe_fd[2];
}public_pipe;

typedef struct {
	char user_name[ClientNum][20];
	int user_fd_table[ClientNum];
	char user_environpath[ClientNum][MaxPATH][MaxPathLen];
	int user_ensizeTable[ClientNum];
	struct sockaddr_in user_ip[ClientNum];
	public_pipe p_pipe[100];
}User_info;

int connectsock(char *service, char *protocol);
int linelen(int fd,char *ptr,int maxlen);
void broadcast(User_info user,char *msg);
void clean_array(char array[],int size);
int main(int argc,char *argv[])
{
	char *service;
	int port=5641;
	if (argc==2){
		port=atoi(argv[1]);
		service = argv[1];
	}
	int sockfd;
	int *status=0;
	char Hello[200] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
	char msg[Maxlenline];
	char *pipe_location="/net/gcs/104/0456115/p_pipe/";
	char *postfix = "% ";
	/*initial*/
	fd_set rfds;//read file descriptor set
	fd_set afds;//active file descriptor set
	int fd,nfds;
	
	clearenv();
	chdir("/net/gcs/104/0456115/ras");
	setenv("PATH","bin:.",1);//set environment
	
	sockfd=connectsock(service,"tcp");
	
	nfds = getdtablesize();
	FD_ZERO(&afds);
	FD_SET(sockfd,&afds);
	
	//multi client data
	User_info user;
	for(int i=0;i<ClientNum;i++){
		user.user_fd_table[i]=-1;
	}
	for(int i=0;i<100;i++){
		user.p_pipe[i].valid=0;
		user.p_pipe[i].exist=0;
	}
	printf("SERVER_PORT: %d\n",port);
	
	superpipe super_pipe[ClientNum][1000];
	for(int i=0;i<ClientNum;i++){
		for(int j=0;j<1000;j++){
			super_pipe[i][j].count=0;
			super_pipe[i][j].valid=0;
		}
	}
	
	/*struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);*/

	for(;;){
		memcpy(&rfds,&afds,sizeof(rfds));
		
		select(nfds,&rfds,(fd_set *)0,(fd_set *)0,(struct timeval *)0);
		if(FD_ISSET(sockfd,&rfds)){
			int ssock;
			struct sockaddr_in client_addr;
			
			int temp_index;
			char ip[Maxlenline];
			char welmsg[Maxlenline];
			for(int i=0;i<ClientNum;i++){
				if(user.user_fd_table[i]==-1){
					temp_index=i;
					break;
				}	
			}
			
			int addrlen = sizeof(user.user_ip[temp_index]);
			ssock = accept(sockfd,(struct sockaddr*)&user.user_ip[temp_index],&addrlen);
			if(ssock < 0) printf("accept error\n");
			
			strcpy(user.user_name[temp_index],"(no name)");
			user.user_ensizeTable[temp_index]=0;
			strcpy(user.user_environpath[temp_index][user.user_ensizeTable[temp_index]],"PATH=bin:.");
			user.user_ensizeTable[temp_index]++;
			user.user_fd_table[temp_index]=ssock;
			
			FD_SET(ssock,&afds);
			write(ssock,Hello,strlen(Hello));
			//get ip
			//inet_ntop(AF_INET, (void *)(&user.user_ip.sin_addr.s_addr), ip, sizeof(ip));    //  convert IPv4 and IPv6 addresses from binary to text form
			//strcat(ip,"/");
			//sprintf(port, "%u", ntohs(user->user_ip.sin_port));
			//strcat(ip,port);
			strcpy(ip, "CGILAB/511");
			sprintf(welmsg,"*** User '%s' entered from %s. ***\n",user.user_name[temp_index],ip);
			broadcast(user,welmsg);
			write(ssock,postfix,strlen(postfix));
		}
		
		for(fd=0;fd<nfds;++fd){
			if(fd != sockfd && FD_ISSET(fd,&rfds)){
				char buf[Maxlenline];
				char commands[Maxlenline];
				int cc,clientfd;
				int user_id,user_index;
				
				clientfd=fd;
				
				dup2(clientfd,fileno(stdout));
				dup2(clientfd,fileno(stderr));
				
				for(int i=0;i<ClientNum;i++){
					if(user.user_fd_table[i]==clientfd){
						user_index=i;
						user_id=user_index+1;
						break;
					}
				}		
				clearenv();
				for(int i=0; i<user.user_ensizeTable[user_index];i++){
					putenv(user.user_environpath[user_index][i]);
				}
				cc=linelen(clientfd,buf,sizeof(buf));
				strcpy(commands,buf);
				
				
				if(strchr(commands,'/')!=0){//To find '/' 
					strcpy(msg,"The character '/' is not be allowed\n");
					write(clientfd,msg,strlen(msg));
				}
				else if(strstr(commands,"exit")!=0){//finished
					
					char exitmsg[Maxlenline];
					sprintf(exitmsg,"*** User '%s' left. ***\n",user.user_name[user_index]);
					broadcast(user,exitmsg);
					
					user.user_fd_table[user_index]=-1;
					FD_CLR(clientfd,&afds);
					shutdown(clientfd,2);
					close(clientfd);
					
					continue;
				}
				else if(strstr(commands,"name")!=0){
					char *str;
					char namemsg[Maxlenline];
					char name[20];
					int name_exist=0;
					str=strtok(commands," \r\n");//command
					str=strtok(NULL," \r\n");//name
					strcpy(name,str);
					
					//check name is existed or not
					for(int i=0;i<30;i++){
						if(user.user_fd_table[i] != -1 && strcmp(user.user_name[i],name)==0){
							name_exist=1;
							break;
						}
					}
					if(name_exist==0){
						strcpy(user.user_name[user_index],name);
						sprintf(namemsg,"*** User from CGILAB/511 is named '%s'. ***\n",name);
						broadcast(user,namemsg);
					}
					else{
						sprintf(namemsg,"*** User '%s' already exists. ***\n",name);
						write(clientfd,namemsg,strlen(namemsg));
					}
					
				}
				else if(strstr(commands,"who")!=0){
					char whomsg[Maxlenline];
					strcpy(whomsg,"<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
					write(clientfd,whomsg,strlen(whomsg));
					for(int i=0; i<ClientNum;i++){
						if(user.user_fd_table[i]!=-1){
							char info[Maxlenline];
							char who_ip[Maxlenline];
							strcpy(who_ip,"CGILAB/511");
							sprintf(info,"%d\t%s\t%s",i+1,user.user_name[i],who_ip);
							if(i==user_index){
								strcat(info,"\t<-me");
							}
							strcat(info,"\n");
							write(clientfd,info,strlen(info));
						}
					}
				}
				else if(strstr(commands,"yell")!=0){
					char yellmsg[Maxlenline];
					char *str;
					str=strtok(commands," \r\n");//yell
					str=strtok(NULL,"\r\n");//msg
					sprintf(yellmsg,"*** %s yelled ***: %s\n",user.user_name[user_index],str);
					broadcast(user,yellmsg);
					
				}
				else if(strstr(commands,"tell")!=0){
					char tellmsg[Maxlenline];
					int tell_id;
					char *str;
					str=strtok(commands," \r\n");//tell
					str=strtok(NULL," \r\n");//tell id
					tell_id=atoi(str);
					str=strtok(NULL,"\r\n");//msg
					
					//check id exist
					if(user.user_fd_table[tell_id-1]!=-1){
						sprintf(tellmsg,"*** %s told you ***: %s\n",user.user_name[user_id-1],str);
						write(user.user_fd_table[tell_id-1],tellmsg,strlen(tellmsg));
					}
					else{//not exist
						sprintf(tellmsg,"*** Error: user #%d does not exist yet. ***\n",tell_id);
						write(clientfd,tellmsg,strlen(tellmsg));
					}
					
				}
				else if(strstr(commands,"setenv")!=0){
					char *str;
					char pathname[CommandLen];
					char path[CommandLen];
					char en_string[CommandLen];
					int exist=-1;
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
							strcpy(en_string,pathname);
							strcat(en_string,"=");
							strcat(en_string,str);
							strcpy(path,str);
							setenv(pathname,path,1);
							for(int i=0 ; i<user.user_ensizeTable[user_index] ;i++){//path exist or not
								if(strstr(user.user_environpath[user_index][i],pathname)!=0){
									exist=i;
									break;
								}
							}
							
							if(exist==-1){
								strcpy(user.user_environpath[user_index][user.user_ensizeTable[user_index]],en_string);
								user.user_ensizeTable[user_index]++;
							}
							else{
								strcpy(user.user_environpath[user_index][exist],en_string);
							}
						}
					}	
				}
				else if(strstr(commands,"printenv")!=0){
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
					char *ch;
					char pipe_writepath[Maxlenline];
					char pipe_readpath[Maxlenline];
					clean_array(pipe_writepath,Maxlenline);
					clean_array(pipe_readpath,Maxlenline);
					int pipe_read=0;
					int pipe_readnum=0;
					int pipe_write=0;
					int pipe_writenum=0;
					int public_pipe_err=0;
					if((ch=strchr(commands,'>'))!=NULL){//write
						if(isdigit(ch[1])){
							pipe_write=1;
							pipe_writenum=atoi(ch+1);
							char writenum[1];
							sprintf(writenum,"%d",pipe_writenum);
							strcpy(pipe_writepath,pipe_location);
							strcat(pipe_writepath,writenum);
							if(user.p_pipe[pipe_writenum].exist==1){
								char pipe_existmsg[Maxlenline];
								sprintf(pipe_existmsg,"*** Error: public pipe #%d already exists. ***\n",pipe_writenum);
								write(clientfd,pipe_existmsg,strlen(pipe_existmsg));
								public_pipe_err=1;
							}
						}
					}
					if((ch=strchr(commands,'<'))!=NULL){//read
						if(isdigit(ch[1])){
							pipe_read=1;
							pipe_readnum=atoi(ch+1);
							char readnum[1];
							sprintf(readnum,"%d",pipe_readnum);
							strcpy(pipe_readpath,pipe_location);
							strcat(pipe_readpath,readnum);
							if(user.p_pipe[pipe_readnum].valid==0){
								char pipe_noexistmsg[Maxlenline];
								sprintf(pipe_noexistmsg,"*** Error: public pipe #%d does not exist yet. ***\n",pipe_readnum);
								write(clientfd,pipe_noexistmsg,strlen(pipe_noexistmsg));
								public_pipe_err=1;
							}
						}
					}
					if(public_pipe_err==0){
						char *str;
						char pathtmp[CommandLen];
						char path[CommandLen];
						
						char commandmsg[Maxlenline];
						clean_array(commandmsg,Maxlenline);
						strcpy(commandmsg,commands);
						commandmsg[strlen(commandmsg)-1]='\0';
						int fileread_fd;
						int filewrite_fd;
						
						
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
										str=strtok(NULL," \t\n\r");
										strcpy(filename,str);
									}
									else if(strchr(str,'>')!=NULL){// ls >3
										while(1)break;//noop
									}
									else if(strchr(str,'<')!=NULL){// cat <2
										while(1)break;//noop
									}
									else{
										parameters[num_parameter]=str;
										num_parameter++;
									}
									while((str=strtok(NULL," \t\n\r"))!= NULL){//first parameter
											if(strcmp(str,">")==0){
											filewrite=1;
											str=strtok(NULL," \t\n\r");
											strcpy(filename,str);
										}
										else if(strchr(str,'>')!=NULL){//cat test.html >3
											while(1)break;//noop
										}
										else if(strchr(str,'<')!=NULL){//cat test.html >3
											while(1)break;//noop
										}
										else{
											parameters[num_parameter]=str;
											num_parameter++;
										}
									}
									parameters[num_parameter]=(char *)NULL;
									
									int countequal0=-1;
									for(int i=0;i<1000;i++){
										if(super_pipe[user_index][i].count > 0 && super_pipe[user_index][i].valid ==1)super_pipe[user_index][i].count--;
									}
									for(int i=0;i<1000;i++){
										if(super_pipe[user_index][i].count==0 && super_pipe[user_index][i].valid ==1){//get the number of pipe
											countequal0=i;
											break;
										}
									}
									
									if(pipe_write==1){
										pipe(user.p_pipe[pipe_writenum].pipe_fd);
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
											if(pipe_read==1 && pipe_write==1){
												
												char pp_readmsg[Maxlenline];
												sprintf(pp_readmsg,"*** %s (#%d) just received via '%s' ***\n",user.user_name[user_index],user_id,commandmsg);
												broadcast(user,pp_readmsg);
												
												///////////////////////////////////////////////////////////////
												
												char pp_writemsg[Maxlenline];
												sprintf(pp_writemsg,"*** %s (#%d) just piped '%s' ***\n",user.user_name[user_index],user_id,commandmsg);
												broadcast(user,pp_writemsg);
												
												dup2(user.p_pipe[pipe_readnum].pipe_fd[0],fileno(stdin));
												dup2(user.p_pipe[pipe_writenum].pipe_fd[1],fileno(stdout));
												close(user.p_pipe[pipe_writenum].pipe_fd[0]);
											}
											else if(pipe_write==1 && pipe_read==0){
												
												char pp_writemsg[Maxlenline];
												sprintf(pp_writemsg,"*** %s (#%d) just piped '%s' ***\n",user.user_name[user_index],user_id,commandmsg);
												broadcast(user,pp_writemsg);
												
												dup2(user.p_pipe[pipe_writenum].pipe_fd[1],fileno(stdout));
												close(user.p_pipe[pipe_writenum].pipe_fd[0]);
											}
											else if(pipe_read==1 && pipe_write==0){
												
												char pp_readmsg[Maxlenline];
												sprintf(pp_readmsg,"*** %s (#%d) just received via '%s' ***\n",user.user_name[user_index],user_id,commandmsg);
												broadcast(user,pp_readmsg);
												
												dup2(user.p_pipe[pipe_readnum].pipe_fd[0],fileno(stdin));
												dup2(clientfd,fileno(stdout));
											}
											else{
												dup2(clientfd,fileno(stdout));
											}
										}
										if(countequal0>=0){
											dup2(super_pipe[user_index][countequal0].pipe_fd[0],fileno(stdin));//++
											close(super_pipe[user_index][countequal0].pipe_fd[1]);
										}
										execv(path,parameters);
										exit(1);
									}
									else{//parent process
										if(countequal0 >=0){
											close(super_pipe[user_index][countequal0].pipe_fd[1]);
										}
										if(pipe_write==1){
											user.p_pipe[pipe_writenum].valid=1;
											user.p_pipe[pipe_writenum].exist=1;
											close(user.p_pipe[pipe_writenum].pipe_fd[1]);
										}
										wait(status);
									}
									if(countequal0 >=0){
										super_pipe[user_index][countequal0].valid=0;
										close(super_pipe[user_index][countequal0].pipe_fd[0]);
									}
									if(pipe_read==1){
										close(user.p_pipe[pipe_readnum].pipe_fd[0]);
										user.p_pipe[pipe_readnum].exist=0;
										user.p_pipe[pipe_readnum].valid=0;
									}
								}
								else{//execution with no parameters
									
									int countequal0= -1;
									for(int i=0;i<1000;i++){
										if(super_pipe[user_index][i].count > 0 && super_pipe[user_index][i].valid ==1)super_pipe[user_index][i].count--;
									}
									for(int i=0;i<1000;i++){
										if(super_pipe[user_index][i].count==0 && super_pipe[user_index][i].valid ==1){//get the number of pipe
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
											dup2(super_pipe[user_index][countequal0].pipe_fd[0],fileno(stdin));//++
											close(super_pipe[user_index][countequal0].pipe_fd[1]);
										}
										dup2(clientfd,fileno(stdout));
										execv(path,parameters);
										exit(1);
									}
									else{
										if(countequal0 >=0){
											close(super_pipe[user_index][countequal0].pipe_fd[1]);
										}
										wait(status);
									}
									if(countequal0 >=0){
										super_pipe[user_index][countequal0].valid=0;
										close(super_pipe[user_index][countequal0].pipe_fd[0]);
									}
								}
							}
							else{//error cmd
								char cmderrmsg[CommandLen+30]="Unknown command: [";
								strcat(cmderrmsg,str);
								strcat(cmderrmsg,"].");
								strcat(cmderrmsg,"\n");
								write(clientfd,cmderrmsg,strlen(cmderrmsg));
							}
						}
					}
				}
				else{//pipe
					char *ch;
					char pipe_writepath[Maxlenline];
					char pipe_readpath[Maxlenline];
					clean_array(pipe_writepath,Maxlenline);
					clean_array(pipe_readpath,Maxlenline);
					int pipe_read=0;
					int pipe_readnum=0;
					int pipe_write=0;
					int pipe_writenum=0;
					int public_pipe_err=0;
					char commandmsg[Maxlenline];
					clean_array(commandmsg,Maxlenline);
					strcpy(commandmsg,commands);
					commandmsg[strlen(commandmsg)-1]='\0';
					if((ch=strchr(commands,'>'))!=NULL){//write public pipe
						if(isdigit(ch[1])){
							pipe_write=1;
							pipe_writenum=atoi(ch+1);
							char writenum[1];
							sprintf(writenum,"%d",pipe_writenum);
							strcpy(pipe_writepath,pipe_location);
							strcat(pipe_writepath,writenum);
							if(user.p_pipe[pipe_writenum].exist==1){
								char pipe_existmsg[Maxlenline];
								sprintf(pipe_existmsg,"*** Error: public pipe #%d already exists. ***\n",pipe_writenum);
								write(clientfd,pipe_existmsg,strlen(pipe_existmsg));
								public_pipe_err=1;
							}
						}
					}
					if((ch=strchr(commands,'<'))!=NULL){//read public pipe
						if(isdigit(ch[1])){
							pipe_read=1;
							pipe_readnum=atoi(ch+1);//++++++++++++++++++++++++++
							char readnum[1];
							sprintf(readnum,"%d",pipe_readnum);
							strcpy(pipe_readpath,pipe_location);
							strcat(pipe_readpath,readnum);
							if(user.p_pipe[pipe_readnum].valid==0){
								char pipe_noexistmsg[Maxlenline];
								sprintf(pipe_noexistmsg,"*** Error: public pipe #%d does not exist yet. ***\n",pipe_readnum);
								write(clientfd,pipe_noexistmsg,strlen(pipe_noexistmsg));
								public_pipe_err=1;
							}
						}
					}
					
					if(public_pipe_err==0){
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
							if(super_pipe[user_index][i].count > 0 && super_pipe[user_index][i].valid ==1)super_pipe[user_index][i].count--;
						}
						for(int i=0;i<1000;i++){
							if(super_pipe[user_index][i].count==0 && super_pipe[user_index][i].valid ==1){//get the number of pipe
								countequal0=i;
								close(super_pipe[user_index][i].pipe_fd[1]);
								break;
							}
						}
						int pipe_fd[2];
						int current_read;
						int current_read_temp;
						//int fileread_fd;
						//int filewrite_fd;
						
						if(pipe_write==1) pipe(user.p_pipe[pipe_writenum].pipe_fd);						
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
										if(super_pipe[user_index][i].count > 0 && super_pipe[user_index][i].valid==1)super_pipe[user_index][i].count++;
									}
									if(ispipe_stderr==1){
										close(pipe_err[0]);
										close(pipe_err[1]);
									}
									if(i>0){
										close(current_read_temp);
									}
									char cmderrmsg[CommandLen+30]="Unknown command: [";
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
											cmd=strtok(NULL," \t\n\r");
											strcpy(filename,cmd);
										}
										else if(strchr(cmd,'>')!=NULL){// ls >3
											while(1)break;//noop
										}
										else if(strchr(cmd,'<')!=NULL){// cat <2
											while(1)break;//noop
										}
										else{
											parameters[num_parameter]=cmd;
											num_parameter++;
										}
										while((cmd = strtok(NULL, " \t\n\r")) != NULL){
											if(strcmp(cmd,">")==0){
												filewrite=1;
												cmd=strtok(NULL," \t\n\r");
												strcpy(filename,cmd);
											}
											else if(strchr(cmd,'>')!=NULL){//cat test.html >3
												while(1)break;//noop
											}
											else if(strchr(cmd,'<')!=NULL){//cat test.html >3
												while(1)break;//noop
											}
											else{
												parameters[num_parameter]=cmd;
												num_parameter++;
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
											else if(pipe_read==1 && i==0){//++++  <
												
												char pp_readmsg[Maxlenline];
												sprintf(pp_readmsg,"*** %s (#%d) just received via '%s' ***\n",user.user_name[user_index],user_id,commandmsg);
												broadcast(user,pp_readmsg);
											
												//fileread_fd=open(pipe_readpath,O_RDWR, 0777);
												//dup2(fileread_fd,fileno(stdin));
												dup2(user.p_pipe[pipe_readnum].pipe_fd[0],fileno(stdin));
												dup2(pipe_fd[1],fileno(stdout));
												close(pipe_fd[0]);
											}
											else if(pipe_write==1 && i==cmd_num-1){//++++  >
												
												char pp_writemsg[Maxlenline];
												sprintf(pp_writemsg,"*** %s (#%d) just piped '%s' ***\n",user.user_name[user_index],user_id,commandmsg);
												broadcast(user,pp_writemsg);
												
												dup2(current_read,fileno(stdin));
												//filewrite_fd=open(pipe_writepath,O_RDWR|O_CREAT, 0777);
												//dup2(filewrite_fd,fileno(stdout));
												dup2(user.p_pipe[pipe_writenum].pipe_fd[1],fileno(stdout));
												close(user.p_pipe[pipe_writenum].pipe_fd[0]);
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
											if(pipe_write==1 && i==cmd_num-1){
												pipe_write=0;
												user.p_pipe[pipe_writenum].valid=1;
												user.p_pipe[pipe_writenum].exist=1;
												close(user.p_pipe[pipe_writenum].pipe_fd[1]);
												//close(filewrite_fd);
											}
											if(pipe_read==1 && i==0){
												pipe_read=0;
												user.p_pipe[pipe_readnum].exist=0;
												user.p_pipe[pipe_readnum].valid=0;
												close(user.p_pipe[pipe_readnum].pipe_fd[0]);
												//remove(pipe_readpath);
												//close(fileread_fd);
											}
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
														dup2(super_pipe[user_index][countequal0].pipe_fd[0],fileno(stdin));//++
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
												close(super_pipe[user_index][countequal0].pipe_fd[0]);
												super_pipe[user_index][countequal0].valid=0;
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
												if(num==super_pipe[user_index][j].count && super_pipe[user_index][j].valid==1){
													close(pipe_err[1]);
													read(pipe_err[0],temp_pipe,sizeof(temp_pipe)*30000);
													read(super_pipe[user_index][j].pipe_fd[0],temp_pipe2,sizeof(temp_pipe2)*30000);
													strcat(temp_pipe2,temp_pipe);
													write(super_pipe[user_index][j].pipe_fd[1],temp_pipe2,strlen(temp_pipe2));
													already=1;
													close(pipe_err[0]);
													break;
												}
											}
											for(int j=0;j<1000;j++){
												if(already==1)break;
												if(super_pipe[user_index][j].valid==0){
													close(pipe_err[1]);
													super_pipe[user_index][j].count=num;
													super_pipe[user_index][j].valid=1;
													pipe(super_pipe[user_index][j].pipe_fd);
													read(pipe_err[0],temp_pipe,sizeof(temp_pipe)*30000);
													write(super_pipe[user_index][j].pipe_fd[1],temp_pipe,strlen(temp_pipe));
													close(pipe_err[0]);
													break;
												}
											}
										}
										else{//|
											int already=0;
											for(int j=0;j<1000;j++){//find the same count
												if(num==super_pipe[user_index][j].count && super_pipe[user_index][j].valid==1){
													read(current_read,temp_pipe,sizeof(temp_pipe)*30000);
													close(current_read);
													read(super_pipe[user_index][j].pipe_fd[0],temp_pipe2,sizeof(temp_pipe2)*30000);
													strcat(temp_pipe2,temp_pipe);
													write(super_pipe[user_index][j].pipe_fd[1],temp_pipe2,strlen(temp_pipe2));
													already=1;
													break;
												}
											}
											for(int j=0;j<1000;j++){
												if(already==1)break;
												if(super_pipe[user_index][j].valid==0){
													super_pipe[user_index][j].count=num;
													super_pipe[user_index][j].valid=1;
													pipe(super_pipe[user_index][j].pipe_fd);
													read(current_read,temp_pipe,sizeof(temp_pipe)*30000);
													close(current_read);
													write(super_pipe[user_index][j].pipe_fd[1],temp_pipe,strlen(temp_pipe));
													break;
												}
											}
										}
									}
									else{//only |
										int already=0;
										for(int j=0;j<1000;j++){//find the same count
											if(num==super_pipe[user_index][j].count && super_pipe[user_index][j].valid==1){
												read(current_read,temp_pipe,sizeof(temp_pipe)*30000);
												close(current_read);
												read(super_pipe[user_index][j].pipe_fd[0],temp_pipe2,sizeof(temp_pipe2)*30000);
												strcat(temp_pipe2,temp_pipe);
												write(super_pipe[user_index][j].pipe_fd[1],temp_pipe2,strlen(temp_pipe2));
												already=1;
												break;
											}
										}
										for(int j=0;j<1000;j++){
											if(already==1)break;
											if(super_pipe[user_index][j].valid==0){
												super_pipe[user_index][j].count=num;
												super_pipe[user_index][j].valid=1;
												pipe(super_pipe[user_index][j].pipe_fd);
												read(current_read,temp_pipe,sizeof(temp_pipe)*30000);
												close(current_read);
												write(super_pipe[user_index][j].pipe_fd[1],temp_pipe,strlen(temp_pipe));
												break;
											}
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
int connectsock(char *service, char *protocol)
{
	int s,type;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in dest;
	int portbase = 50000;
	
	bzero((char *)&dest,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = INADDR_ANY;
	//dest.sin_port = htons(port);
	
	if(pse = getservbyname(service,protocol)){
		//dest.sin_port = htons(port);
		dest.sin_port=htons(ntohs((u_short)pse->s_port) + portbase);
	}
	else if((dest.sin_port = htons((u_short)atoi(service)))==0){
		exit(1);
	}
	if((ppe = getprotobyname(protocol)) == 0){
        fprintf(stderr, "can't get \"%s\" protocol entry\n", protocol); 
        fflush(stderr);
        exit(1);
    }
	
	if(strcmp(protocol,"udp")==0 ) {
		type = SOCK_DGRAM;
	}
	else{
		type = SOCK_STREAM;
	}
	
	s = socket(PF_INET,type,ppe->p_proto);
	bind(s, (struct sockaddr *)&dest, sizeof(dest));
	listen(s, ClientNum);
	
	return s;
}
void broadcast(User_info user,char *msg)
{
	for(int i=0;i<ClientNum;i++){
		if(user.user_fd_table[i]!=-1){
			write(user.user_fd_table[i],msg,strlen(msg));
		}
	}
}
void clean_array(char array[],int size)
{
	for(int i=0 ; i<size;i++){
		array[i]='\0';
	}
}
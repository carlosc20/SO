#include "Includes/main.h"
#include "Includes/btree.h"
#include <sys/wait.h>

#include <signal.h>
#include <sys/types.h>

#define BUFFER 512

void noArgs(int argc){
	if(argc < 2){
		fprintf(stderr, "use: Bash <path>\n");
		exit(1);
	}
}

int openNoteBook(char *path){
	int fd;
	if((fd = open(path, O_RDWR, 0644)) < 0){
		write(2, "Error On Opening File!\n", 24);
	}

	return fd;
}

void execCMD(DynaArray *ans, char *cmd){
	char *str = strdup(cmd);
	char *string = str;
	int index;

	int p[2];
	pipe(p);
	dup2(p[0], 0);
	close(p[0]);

	while(str[0] == ' '){
		str++;
	}

	if(sscanf(str, "%d|", &index) == 1){
		if(index > ans->length){
			char *error = malloc(31 + strlen(str));
			strcpy(error, "Error on executing command: $");
			strcat(error, str);
			strcat(error, "\n");
			write(2, error, strlen(error));
			exit(1);
		} else {
			str = strstr(str, "|");
			str++;

			write(p[1], ans->array[ans->length - index], strlen(ans->array[ans->length - index]));
		}
	} else if(str[0] == '|'){
		str++;

		write(p[1], ans->array[ans->length - 1], strlen(ans->array[ans->length - 1]));
	}

	close(p[1]);
	fprintf(stderr, "%s\n", str);
	execute(str);
	free(string);
}

void callCMDS(DynaArray *cmds, DynaArray *ans){
	for(int i = 0; i < cmds->length; i++){
		int p[2];
		int status;
		pipe(p);

		if(fork()){
			wait(&status);
			if(status != 0){
				exit(2);
			}

			close(p[1]);
			insertDynaArrayNoCpy(ans, readFile(p[0]));
			close(p[0]);
		} else {

			close(p[0]);
			dup2(p[1],1);
			close(p[1]);

			execCMD(ans, cmds->array[i]);

			exit(0);
		}
	}
}

void handler(int sig) {
	printf("control_c\n");
}

/*
Cria um notebook novo já processado e apaga o original
*/
void replaceNotebook(char *notebook, char *str){
	/*
	signal(SIGINT, handler);//^C
	signal(signum, SIG_IGN) -> ignora o sinal
	signal(signum, SIG_DEFAULT) -> volta ao normal
	*/

	char *nb = strdup(notebook);

	//isola nome do ficheiro
	while(strstr(nb, "/")){
		nb = strstr(nb, "/");
		nb++;
	}

	char *path = strndup(notebook, strlen(notebook) - strlen(nb));

	char *sub = malloc(strlen(path) + strlen(nb) + 4);
	strcpy(sub, path);
	strcat(sub, "TMP");
	strcat(sub, nb);

	int fd = open(sub, O_WRONLY, 0644);
	if(fd < 0){
		fd = open(sub, O_CREAT | O_RDWR, 0644);
		if(write(fd, str, strlen(str)) < 0){
			write(2, "Error on writing to TMP file!\n", 31);
			if(unlink(sub)){
				write(2, "Error on Deleting TMP file!\n", 29);
			}
			exit(1);
		}

		if(unlink(notebook)){
			write(2, "Error on Deleting notebook file!\n", 34);
			if(unlink(sub)){
				write(2, "Error on Deleting TMP file!\n", 29);
			}
			exit(2);
		}

		if(link(sub, notebook)){
			write(2, "Error on Renaming TMP file!\n", 29);
		}

		if(unlink(sub)){
			write(2, "Error on Deleting TMP file!\n", 29);
		}
	}
}

char *combine(DynaArray *descs, DynaArray *cmds, DynaArray *ans){
	int i = 0;
	char *str = malloc(1);

	while(i < cmds->length){

		str = realloc(str, strlen(str) + strlen(descs->array[i]) + strlen(cmds->array[i]) + strlen(ans->array[i]) + 10);

		strcat(str, descs->array[i]);
		strcat(str, "$");
		strcat(str, cmds->array[i]);
		strcat(str, "\n>>>\n");
		strcat(str, ans->array[i]);
		strcat(str, "<<<\n");

		i++;
	}
	return str;
}

int openNotebook(char *path){
	int fd;
	if((fd = open(path, O_RDONLY, 0644)) < 0){
		write(2, "Error On Opening NoteBook!\n", 24);
	}
	return fd;
}


int main(int argc, char *argv[]){

	if(argc < 2){
		fprintf(stderr, "use: Bash <path>\n");
		exit(1);
	}

	//Array dinamico para guardar os comandos(incluindo a descriçao do mesmo cmd) e os resultados
	DynaArray *cmds = createDynaArray(10);
	DynaArray *descs = createDynaArray(10);
	DynaArray *ans = createDynaArray(10);

	//Le o ficheiro (notebook.txt) e insere no array cmds os comandos
	int fd = openNotebook(argv[1]);
	separateCMD(cmds, descs, readFile(fd));
	close(fd);

	//Chamada um comando de cada vez ordenadamente usando o fork e executa-o, escrevendo o resultado num array dinamico (ans)
	callCMDS(cmds, ans);

	//cria um notebook com as respostas que depois substitui o original
	replaceNotebook(argv[1], combine(descs, cmds, ans));

	return 0;
}

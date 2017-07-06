#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#define BUFLEN    1024     /* コマンド用のバッファの大きさ */
#define MAXARGNUM  256     /* 最大の引数の数 */
#define HISTORY_SIZE 32
#define DIRSIZE 256
#define PROMPT_SIZE 64
#define KEYSIZE 32

struct stack{
	char dir[DIRSIZE];
	struct stack *next;
};
typedef struct stack STACK;

struct pairlist{
	char command1[KEYSIZE];
	char command2[KEYSIZE];
	struct pairlist *next;
};
typedef struct pairlist ALIAS;

char prompt[PROMPT_SIZE]="Command : ";
char *command_history[HISTORY_SIZE+1]={0};
STACK *dirhead=NULL;
ALIAS *aliashead=NULL;

int parse(char[],char*[]);
void wild_card(char[]);
void add_history(char []);
void execute_command(char*[],int);
int execute_native_command(char*[],int);
void change_directory(char[]);
void push_directory(void);
void dirs(void);
void pop_directory(void);
void history(void);
int search_history(char[]);
void change_prompt(char[]);
void alias(char*[]);
void unalias(char[]);
void clean(void);


int main(int argc,char *argv[]){
	char command_buffer[BUFLEN]; /* コマンド用のバッファ */
	char *args[MAXARGNUM];       /* 引数へのポインタの配列 */
	int command_status;          /* コマンドの状態を表す
								 command_status = 0 : フォアグラウンドで実行
								 command_status = 1 : バックグラウンドで実行
								 command_status = 2 : シェルの終了
								 command_status = 3 : 何もしない */
	int isscript=0;

	if(isatty(fileno(stdin))==0){
		if(errno==ENOTTY){
			isscript=1;
		}
	}

	for(;;){
		if(!isscript){//スクリプト実行中でなければ
			printf("%s",prompt); //プロンプトを表示する
		}

		// 標準入力から１行を command_buffer へ読み込む
		// 入力が何もなければスクリプトの終わりなのでループ脱出
		if(fgets(command_buffer,BUFLEN,stdin) == NULL){
			break;
		}

		//  入力されたバッファ内のコマンドを解析する
		//  返り値はコマンドの状態
		command_status = parse(command_buffer,args);

		//  終了コマンドならばループ脱出
		//  引数が何もなければプロンプト表示へ戻る
		if(command_status == 2){
			break;
		}else if(command_status == 3){
			continue;
		}
		execute_command(args,command_status);// コマンドを実行する
	}

	clean();
	return 0;
}

/*----------------------------------------------------------------------------
*
*  関数名   : parse
*
*  作業内容 : バッファ内のコマンドと引数を解析する
*
*  引数     :
*
*  返り値   : コマンドの状態を表す :
*                0 : フォアグラウンドで実行
*                1 : バックグラウンドで実行
*                2 : シェルの終了
*                3 : 何もしない
*
*  注意     :
*
*--------------------------------------------------------------------------*/

int parse(char buffer[],char *args[]){
	int arg_index=0;   /* 引数用のインデックス */
	int status=0;   /* コマンドの状態を表す */

	*(buffer + (strlen(buffer) - 1)) = '\0'; //バッファ内の最後にある改行をヌル文字へ変更

	if(buffer[0]=='!'){//!から始まる呼び出しをhistoryに含めないため先に実行
		if(search_history(buffer)==0){

			status = 3;
			return status;
		}
		++buffer;//!を削る
		printf("%s\n",buffer);
	}

	add_history(buffer);
	//  バッファが終了を表すコマンド（"exit"）ならば
	//  コマンドの状態を表す返り値を 2 に設定してリターンする
	if(strcmp(buffer,"exit") == 0){
		status = 2;
		return status;
	}


	//wild_card(buffer);

	//  バッファ内の文字がなくなるまで繰り返す
	//  （ヌル文字が出てくるまで繰り返す）
	while(*buffer != '\0'){
		//  空白類（空白とタブ）をヌル文字に置き換える  これによってバッファ内の各引数が分割される
		while(*buffer == ' ' || *buffer == '\t'){
			*(buffer++) = '\0';
		}

		if(*buffer == '\0'){// 空白の後が終端文字であればループを抜ける
			break;
		}

		// buffer は現在は arg_index + 1 個めの引数の先頭を指している
		// 引数の先頭へのポインタを引数へのポインタ配列に格納する
		args[arg_index] = buffer;
		++arg_index;

		//  引数部分を読み飛ばす（ヌル文字でも空白類でもない場合に読み進める）
		while((*buffer != '\0') && (*buffer != ' ') && (*buffer != '\t')){
			++buffer;
		}
	}

	args[arg_index] = NULL; // 最後の引数の次にはヌルへのポインタを格納する

	/* 最後の引数をチェックして "&" ならば
	*  "&" を引数から削る
	*  コマンドの状態を表す status に 1 を設定する
	*  そうでなければ status に 0 を設定する*/

	if(arg_index > 0 && strcmp(args[arg_index - 1],"&") == 0){
		--arg_index;
		args[arg_index] = '\0';
		status = 1;
	} else{
		status = 0;
	}

	if(arg_index == 0){ //引数が何もなかった場合
		status = 3;
	}

	return status; //  コマンドの状態を返す
}




void wild_card(char buffer[]){
	char key[KEYSIZE];
	while(*buffer != '\0'){
		if(*buffer=='*'){
			if(*(buffer-1)!=' '||*(buffer-1)!='\t'){// strings*の場合

			}else if(*(buffer+1)!=' '||*(buffer+1)!='\t'){// *stringsの場合

			}else{// *の場合

			}
		}else{
			++buffer;
		}
	}
}

void add_history(char buffer[]){
	int i;
	static int history_size=0;
	char *tmp=malloc(sizeof(char)*(strlen(buffer)+1));
	strcpy(tmp,buffer);

	if(history_size<HISTORY_SIZE){
		command_history[history_size]=tmp;
		++history_size;
	}else{//historyが満タン
		free(command_history[0]);
		for(i=1;i<HISTORY_SIZE;++i){
			command_history[i-1]=command_history[i];
		}
		command_history[HISTORY_SIZE-1]=tmp;
	}
}

/*----------------------------------------------------------------------------
*
*  関数名   : execute_command
*
*  作業内容 : 引数として与えられたコマンドを実行する
*             コマンドの状態がフォアグラウンドならば、コマンドを
*             実行している子プロセスの終了を待つ
*             バックグラウンドならば子プロセスの終了を待たずに
*             main 関数に返る（プロンプト表示に戻る）
*
*  引数     :
*
*  返り値   :
*
*  注意     :
*
*--------------------------------------------------------------------------*/

void execute_command(char *args[],int command_status){
	int pid;      /* プロセスＩＤ */
	int status;   /* 子プロセスの終了ステータス */



	//myshに存在するコマンドならそちらを実行する
	if(execute_native_command(args,command_status)){
		return;
	}

  /*
  *  子プロセスを生成する
  *
  *  生成できたかを確認し、失敗ならばプログラムを終了する
  */

	pid=fork();
	if(pid<0){
		printf("プロセスが生成できませんでした\n");
		exit(1);
	}

  /*
  *  子プロセスの場合には引数として与えられたものを実行する
  *
  *  引数の配列は以下を仮定している
  *  ・第１引数には実行されるプログラムを示す文字列が格納されている
  *  ・引数の配列はヌルポインタで終了している
  */

	if(pid==0){
		printf("子プロセススタート\n");
		execvp(args[0],args);
		printf("子プロセス終了\n");//NOTREACHED
		return;
	}

  /*
  *  コマンドの状態がバックグラウンドなら関数を抜ける
  */

	if(command_status==1){
		return;
	}

  /*
  *  ここにくるのはコマンドの状態がフォアグラウンドの場合
  *
  *  親プロセスの場合に子プロセスの終了を待つ
  */

	if(command_status==0){
		wait(&status);
		printf("子プロセスwait終了\n");
	}

	return;
}

//コマンドがmyshに存在するコマンドなら実行して1を返す
//存在しなければ0を返す
int execute_native_command(char *args[],int command_status){
	if(strcmp(args[0],"cd")==0){
		change_directory(args[1]);
		return 1;
	}
	if(strcmp(args[0],"pushd")==0){
		push_directory();
		return 1;
	}
	if(strcmp(args[0],"dirs")==0){
		dirs();
		return 1;
	}
	if(strcmp(args[0],"popd")==0){
		pop_directory();
		return 1;
	}
	if(strcmp(args[0],"history")==0){
		history();
		return 1;
	}
	if(strcmp(args[0],"prompt")==0){
		change_prompt(args[1]);
		return 1;
	}
	if(strcmp(args[0],"alias")==0){
		alias(args);
		return 1;
	}
	if(strcmp(args[0],"unalias")==0){
		unalias(args[1]);
		return 1;
	}

	return 0;
}

void change_directory(char dir[]){
	errno=0;
	if(dir==NULL){
		dir=getenv("HOME");
	}

	if(chdir(dir)==0){
		printf("Changed directory to %s\n",dir);
	}else{
		printf("Failed to change directory\n",dir);
		if(errno==ENOENT){
			printf("%s doesn't exist\n",dir);
		}
		if(errno==EACCES){
			printf("%s :Permission denied\n",dir);
		}
	}
}

void push_directory(void){
	char dir[DIRSIZE];
	STACK *tmp;
	getcwd(dir,DIRSIZE);
	if(dirhead==NULL){
		dirhead=malloc(sizeof(STACK));
		strcpy(dirhead->dir,dir);
		dirhead->next=NULL;
		printf("Pushed directory %s\n",dir);
		return;
	}

	tmp=malloc(sizeof(STACK));
	strcpy(tmp->dir,dir);
	tmp->next=dirhead;
	dirhead=tmp;
	printf("Pushed directory %s\n",dir);
}

void dirs(void){
	STACK *tmp=dirhead;
	while(NULL!=tmp){
		printf("%s\n",tmp->dir);
		tmp=tmp->next;
	}
}

void pop_directory(void){
	STACK *tmp;
	if(dirhead==NULL){
		printf("No directories in stack\n");
		return;
	}

	chdir(dirhead->dir);
	printf("Pushed directory %s\n",dirhead->dir);
	tmp=dirhead->next;
	free(dirhead);
	dirhead=tmp;
}

void history(void){
	int i;
	for(i=0;command_history[i]!=NULL;++i){
		printf("%2d:%s\n",i+1,command_history[i]);
	}
}

int search_history(char buffer[]){
	int i=0;
	int idx=HISTORY_SIZE;
	++buffer;//最初が!の時に呼ばれるので!を飛ばす
	while(command_history[idx]==NULL){//historyの空の部分を読み飛ばす
		--idx;
		if(idx==-1){
			printf("No commands in history\n");
			return 0;
		}
	}

	if(buffer[0]=='!'){// !!
		//idxはhistoryの最後を指している
		strcpy(buffer,command_history[idx]);
		return 1;
	}else{
		while(idx>=0){
			printf("idx=%d\n",idx);
			if(strncmp(command_history[idx],buffer,strlen(buffer))==0){
				strcpy(buffer,command_history[idx]);
				return 1;
			}
			--idx;
		}
	}

	printf("No such command in history\n");
	return 0;
}

void change_prompt(char arg[]){
	if(arg==NULL){
		strcpy(prompt,"Command : ");
	}else{
		strcpy(prompt,arg);
	}

}

void alias(char *args[]){
	ALIAS *tmp;
	if(args[0]==NULL){
		tmp=aliashead;
		while(tmp!=NULL){
			printf("%s %s\n",tmp->command1,tmp->command2);
		}
		return;
	}

	if(args[1]==NULL){
		printf("no target\n");
		return;
	}

	if(aliashead==NULL){
		aliashead=malloc(sizeof(ALIAS));
		strcpy(aliashead->command1,args[0]);
		strcpy(aliashead->command2,args[1]);
		aliashead->next=NULL;
	}else{
		tmp=malloc(sizeof(ALIAS));
		strcpy(tmp->command1,args[0]);
		strcpy(tmp->command2,args[1]);
		tmp->next=NULL;

	}
}

void unalias(char arg[]){
	ALIAS *tmp=aliashead;
	ALIAS *prev=NULL;
	while(tmp->next!=NULL){
		if(strcmp(tmp->command1,arg)==0){
			if(prev==NULL){//aliasheadが該当していたらaliasheadを移す必要あり
				aliashead=tmp->next;
			}else{//そうでなければつなぎ替えるだけ
				prev->next=tmp->next;
			}
			free(tmp);
			return;
		}
		prev=tmp;
		tmp=tmp->next;
	}
	//NOTFOUND
	printf("No such alias\n");
}

void clean(void){
	int i;
	STACK *tmp1;
	ALIAS *tmp2;
	while(NULL!=dirhead){
		tmp1=dirhead->next;
		free(dirhead);
		dirhead=tmp1;
	}
	for(i=0;i<HISTORY_SIZE;++i){
		free(command_history[i]);
	}
	while(NULL!=aliashead){
		tmp2=aliashead->next;
		free(aliashead);
		aliashead=tmp2;
	}
}

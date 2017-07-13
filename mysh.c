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
typedef struct stack DIRSTACK;

struct pairlist{
	char command1[KEYSIZE];
	char command2[KEYSIZE];
	struct pairlist *next;
};
typedef struct pairlist ALIAS;

char prompt[PROMPT_SIZE] = "Command : ";
char *command_history[HISTORY_SIZE + 1] = { 0 };
DIRSTACK *dirhead = NULL;
ALIAS *aliashead = NULL;

int parse(char[], char*[]);
int wild_card(char[], char*);
int strcmp_r(char[], char[]);
void add_history(char[]);
void execute_command(char*[], int);
int execute_native_command(char*[], int);
void change_directory(char[]);
void push_directory(void);
void dirs(void);
void pop_directory(void);
void history(void);
int search_history(char[]);
void change_prompt(char[]);
void alias(char*[]);
void unalias(char[]);
char* search_alias(char[]);
void word_count(char[]);
void sort(char[]);
void sort_line(char*[], int, int);
void swap(char*[], int, int);
void clean(void);

int main(void){
	char buffer[BUFLEN]; /* コマンド用のバッファ */
	char *args[MAXARGNUM]; /* 引数へのポインタの配列 */
	int command_status; /* コマンドの状態を表す
	 command_status = 0 : フォアグラウンドで実行
	 command_status = 1 : バックグラウンドで実行
	 command_status = 2 : シェルの終了
	 command_status = 3 : 何もしない */
	int isscript = 0;

	if(isatty(fileno(stdin)) == 0){
		if(errno == ENOTTY){
			isscript = 1;
		}
	}

	for(;;){
		if(!isscript){ //スクリプト実行中でなければ
			printf("%s", prompt); //プロンプトを表示する
		}

		// 標準入力から１行を command_buffer へ読み込む
		// 入力が何もなければスクリプトの終わりなのでループ脱出
		if(fgets(buffer, BUFLEN, stdin) == NULL){
			break;
		}

		//  入力されたバッファ内のコマンドを解析する
		//  返り値はコマンドの状態
		command_status = parse(buffer, args);

		//  終了コマンドならばループ脱出
		//  引数が何もなければプロンプト表示へ戻る
		if(command_status == 2){
			break;
		}else if(command_status == 3){
			continue;
		}

		execute_command(args, command_status);		// コマンドを実行する
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

int parse(char buffer[], char *args[]){
	int arg_index = 0; /* 引数用のインデックス */
	int status = 0; /* コマンドの状態を表す */

	*(buffer + (strlen(buffer) - 1)) = '\0'; //バッファ内の最後にある改行をヌル文字へ変更
	char *idx = strchr(buffer, '#');// コメントを無視
	if(idx!=NULL){
		*idx='\0';
	}

	if(buffer[0] == '!'){ //!から始まる呼び出しをhistoryに含めないため先に実行
		if(search_history(buffer) == 0){
			status = 3;
			return status;
		}
		++buffer; //!を削る
		printf("%s\n", buffer); //実際に実行するコマンドを表示
	}

	add_history(buffer);
	//  バッファが終了を表すコマンド（"exit"）ならば
	//  コマンドの状態を表す返り値を 2 に設定してリターンする
	if(strcmp(buffer, "exit") == 0){
		status = 2;
		return status;
	}

	idx = strchr(buffer, '*');
	if(idx != NULL){
		while(*(idx - 1) != ' ' && *(idx - 1) != '\t'){
			--idx;
		}
		//idxはbuffer内の*のついた引数の先頭を指している
		if(wild_card(buffer, idx) == 0){
			status = 3;
			return status;
		}
	}

	//  バッファ内の文字がなくなるまで繰り返す
	//  （ヌル文字が出てくるまで繰り返す）
	while(*buffer != '\0'){
		//  空白類（空白とタブ）をヌル文字に置き換える  これによってバッファ内の各引数が分割される
		while(*buffer == ' ' || *buffer == '\t'){
			*(buffer++) = '\0';
		}

		if(*buffer == '\0'){	// 空白の後が終端文字であればループを抜ける
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

	args[0] = search_alias(args[0]);

	/* 最後の引数をチェックして "&" ならば
	 *  "&" を引数から削る
	 *  コマンドの状態を表す status に 1 を設定する
	 *  そうでなければ status に 0 を設定する*/
	if(arg_index > 0 && strcmp(args[arg_index - 1], "&") == 0){
		--arg_index;
		args[arg_index] = '\0';
		status = 1;
	}else{
		status = 0;
	}

	if(arg_index == 0){ //引数が何もなかった場合
		status = 3;
	}

	return status; //  コマンドの状態を返す
}

//置換できたら1，できなかったら0を返す
int wild_card(char buffer[], char *idx){
	int i;
	char *argidx = 0;
	char exp[BUFLEN] = "";
	char command[BUFLEN] = "";
	char args[BUFLEN] = "";
	char key[KEYSIZE];
	DIR *dir = opendir("./");
	struct dirent *dent;

	//bufferの*のついた引数までを一時コピー
	strncpy(command, buffer, (int)(idx - buffer) / sizeof(char));
	command[(int)(idx - buffer) / sizeof(char)] = '\0';
	//*のついた引数より後を一時コピー
	argidx = idx;
	//*のついた引数を読み飛ばす
	while(*argidx != ' ' && *argidx != '\t'){
		++argidx;
	}
	strcpy(args, argidx);

	if(*idx != '*'){ // strings*の場合
		for(i = 0; idx[i] != '*'; ++i){
			key[i] = idx[i];
		}
		key[i] = '\0';
		while((dent = readdir(dir)) != NULL){
			if(dent->d_type == DT_REG){
				if(strncmp(dent->d_name, key, strlen(key)) == 0){
					strcat(exp, dent->d_name);
					strcat(exp, " ");
				}
			}
		}
	}else if(*(idx + 1) != ' ' && *(idx + 1) != '\t' && *(idx + 1) != '\0'){ // *stringsの場合
		for(i = 1; idx[i] != ' ' && idx[i] != '\t'; ++i){
			key[i - 1] = idx[i];
		}
		key[i - 1] = '\0';
		while((dent = readdir(dir)) != NULL){
			if(dent->d_type == DT_REG){
				if(strcmp_r(dent->d_name, key)){
					strcat(exp, dent->d_name);
					strcat(exp, " ");
				}
			}
		}
	}else{
		while((dent = readdir(dir)) != NULL){
			if(dent->d_type == DT_REG){
				strcat(exp, dent->d_name);
				strcat(exp, " ");
			}
		}
	}

	if(strcmp(exp, "") == 0){
		printf("could not replace\n");
		return 0;
	}

	printf("%s\n", buffer);
	return 1;
}

//str1の末尾がstr2と一致するか確認
//一致すれば1,しなければ0を返す
int strcmp_r(char *str1, char *str2){
	if(strlen(str1) < strlen(str2)){
		//str1の方が短いとき一致することはない
		return 0;
	}
	//str1の検索開始位置を文字数の差ぶんずらす
	str1 += strlen(str1) - strlen(str2);
	if(strcmp(str1, str2) == 0){
		return 1;
	}else{
		return 0;
	}
}

void add_history(char buffer[]){
	int i;
	static int history_size = 0;
	char *tmp = malloc(sizeof(char) * (strlen(buffer) + 1));
	strcpy(tmp, buffer);

	if(history_size < HISTORY_SIZE){
		command_history[history_size] = tmp;
		++history_size;
	}else{ //historyが満タン
		free(command_history[0]);
		for(i = 1; i < HISTORY_SIZE; ++i){
			command_history[i - 1] = command_history[i];
		}
		command_history[HISTORY_SIZE - 1] = tmp;
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

void execute_command(char *args[], int command_status){
	int pid; /* プロセスＩＤ */
	int status; /* 子プロセスの終了ステータス */

	//myshに存在するコマンドならそちらを実行する

	for(int i = 0; args[i] != NULL; ++i){
		printf("args[%d]:%s\n", i, args[i]);
	}
	if(execute_native_command(args, command_status)){
		return;
	}

	/*
	 *  子プロセスを生成する
	 *
	 *  生成できたかを確認し、失敗ならばプログラムを終了する
	 */

	pid = fork();
	if(pid < 0){
		printf("Cannot create process\n");
		exit(1);
	}

	/*
	 *  子プロセスの場合には引数として与えられたものを実行する
	 *
	 *  引数の配列は以下を仮定している
	 *  ・第１引数には実行されるプログラムを示す文字列が格納されている
	 *  ・引数の配列はヌルポインタで終了している
	 */

	if(pid == 0){
		execvp(args[0], args);
		//NOTFOUND
		printf("Command not found\n");
		exit(1);
	}

	/*
	 *  コマンドの状態がバックグラウンドなら関数を抜ける
	 */

	if(command_status == 1){
		return;
	}

	/*
	 *  ここにくるのはコマンドの状態がフォアグラウンドの場合
	 *
	 *  親プロセスの場合に子プロセスの終了を待つ
	 */

	if(command_status == 0){
		wait(&status);
	}

	return;
}

//コマンドがmyshに存在するコマンドなら実行して1を返す
//存在しなければ0を返す
int execute_native_command(char *args[], int command_status){
	if(strcmp(args[0], "cd") == 0){
		change_directory(args[1]);
		return 1;
	}
	if(strcmp(args[0], "pushd") == 0){
		push_directory();
		return 1;
	}
	if(strcmp(args[0], "dirs") == 0){
		dirs();
		return 1;
	}
	if(strcmp(args[0], "popd") == 0){
		pop_directory();
		return 1;
	}
	if(strcmp(args[0], "history") == 0){
		history();
		return 1;
	}
	if(strcmp(args[0], "prompt") == 0){
		change_prompt(args[1]);
		return 1;
	}
	if(strcmp(args[0], "alias") == 0){
		alias(args);
		return 1;
	}
	if(strcmp(args[0], "unalias") == 0){
		unalias(args[1]);
		return 1;
	}
	if(strcmp(args[0], "wc") == 0){
		word_count(args[1]);
		return 1;
	}
	if(strcmp(args[0], "sort") == 0){
		sort(args[1]);
		return 1;
	}

	return 0;
}

void change_directory(char dir[]){
	errno = 0;
	if(dir == NULL){
		dir = getenv("HOME");
	}

	if(chdir(dir) == 0){
		printf("Changed directory to %s\n", dir);
	}else{
		printf("Failed to change directory to %s\n", dir);
		if(errno == ENOENT){
			printf("%s does not exist\n", dir);
		}
		if(errno == EACCES){
			printf("%s :Permission denied\n", dir);
		}
	}
}

void push_directory(void){
	char dir[DIRSIZE];
	DIRSTACK *tmp;
	if(dirhead == NULL){
		dirhead = malloc(sizeof(DIRSTACK));
		getcwd(dirhead->dir, DIRSIZE);
		dirhead->next = NULL;
		printf("Pushed directory %s\n", dirhead->dir);
		return;
	}

	tmp = malloc(sizeof(DIRSTACK));
	getcwd(tmp->dir, DIRSIZE);
	tmp->next = dirhead;
	dirhead = tmp;
	printf("Pushed directory %s\n", dirhead->dir);
}

void dirs(void){
	if(dirhead == NULL){
		printf("No directories in stack\n");
		return;
	}

	DIRSTACK *tmp = dirhead;
	while(NULL != tmp){
		printf("%s\n", tmp->dir);
		tmp = tmp->next;
	}
}

void pop_directory(void){
	DIRSTACK *tmp;
	if(dirhead == NULL){
		printf("No directories in stack\n");
		return;
	}

	chdir(dirhead->dir);
	printf("Poped directory %s\n", dirhead->dir);
	tmp = dirhead->next;
	free(dirhead);
	dirhead = tmp;
}

void history(void){
	int i;
	for(i = 0; command_history[i] != NULL; ++i){
		printf("%2d:%s\n", i + 1, command_history[i]);
	}
}

int search_history(char buffer[]){
	int i = 0;
	int idx = HISTORY_SIZE;
	int num;

	++buffer; //最初が!の時に呼ばれるので!を飛ばす
	while(command_history[idx] == NULL){ //historyの空の部分を読み飛ばす
		--idx;
		if(idx == -1){
			printf("No commands in history\n");
			return 0;
		}
	}
	//idxはhistoryの最後を指している

	num = atoi(buffer);
	if(num > 0){ // !n
		if(command_history[num - 1] != NULL){
			strcpy(buffer, command_history[num - 1]);
			return 1;
		}else{
			printf("!%d: event not found\n", num);
			return 0;
		}
	}
	if(num < 0){ // !-n
		if((idx + num) > -1){
			strcpy(buffer, command_history[idx + num + 1]);
			return 1;
		}else{
			printf("!%d: event not found\n", num);
			return 0;
		}
	}

	if(buffer[0] == '!'){ // !!
		strcpy(buffer, command_history[idx]);
		return 1;
	}else{ // !string
		while(idx >= 0){
			printf("idx=%d\n", idx);
			if(strncmp(command_history[idx], buffer, strlen(buffer)) == 0){
				strcpy(buffer, command_history[idx]);
				return 1;
			}
			--idx;
		}
	}

	printf("No such command in history\n");
	return 0;
}

void change_prompt(char arg[]){
	if(arg == NULL){
		strcpy(prompt, "Command : ");
	}else{
		strcpy(prompt, arg);
	}

}

void alias(char *args[]){
	ALIAS *tmp, *end = aliashead;
	if(args[1] == NULL){
		if(aliashead == NULL){
			printf("no alias\n");
			return;
		}
		tmp = aliashead;
		while(tmp != NULL){
			printf("%s %s\n", tmp->command1, tmp->command2);
			tmp = tmp->next;
		}
		return;
	}

	if(args[2] == NULL){
		printf("no target\n");
		return;
	}

	if(aliashead == NULL){
		aliashead = malloc(sizeof(ALIAS));
		strcpy(aliashead->command1, args[1]);
		strcpy(aliashead->command2, args[2]);
		aliashead->next = NULL;
	}else{
		while(end->next != NULL){
			end++;
		}
		tmp = malloc(sizeof(ALIAS));
		strcpy(tmp->command1, args[1]);
		strcpy(tmp->command2, args[2]);
		tmp->next = NULL;
		end->next = tmp;
	}
}

void unalias(char arg[]){
	ALIAS *tmp = aliashead;
	ALIAS *prev = NULL;

	while(tmp->next != NULL){
		if(strcmp(tmp->command1, arg) == 0){
			if(prev == NULL){ // aliasheadが該当していたらaliasheadを移す必要あり
				aliashead = tmp->next;
			}else{            // そうでなければつなぎ替えるだけ
				prev->next = tmp->next;
			}
			free(tmp);
			return;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	//NOTFOUND
	printf("unalias: %s: not found\n", arg);
}

char* search_alias(char arg[]){
	ALIAS *tmp = aliashead;
	while(tmp != NULL){
		if(strcmp(tmp->command1, arg) == 0){
			return tmp->command2;
		}
		tmp = tmp->next;
	}
	//NOTFOUND
	return arg;
}

void word_count(char filename[]){
	int prev = 0; // 前の文字が区切り文字かどうか
	int lines = 0;
	int words = 0;
	int bytes = 0;
	char c;
	FILE *fp = fopen(filename, "r");

	if(fp == NULL){
		printf("Cannot open file %s", filename);
		return;
	}

	while((c = fgetc(fp)) != '\0'){
		++bytes;
		switch(c){
			case '\n':
				++lines;
				/* no break */
			case ' ':
			case '.':
			case ',':
			case '\r':
				if(!prev){ // 前の文字が区切り文字でなければ
					++words; // 単語数を増やす
				}
				prev = 1;
				break;
			default:
				prev = 0;
				break;
		}
	}

	printf("%8d %8d %8d %s\n", lines, words, bytes, filename);
	fclose(fp);
}

void sort(char filename[]){
	int cl;
	char *line[1024];
	FILE *fp = fopen(filename, "r");

	if(fp == NULL){
		printf("Cannot open file %s", filename);
		return;
	}

	for(cl = 0; cl < 1024; ++cl){
		line[cl] = malloc(256);
		printf("malloc\n");
		if(fgets(line[cl], 255, fp) == NULL){
			break;
		}

		*(line[cl] + (strlen(line[cl]) - 1)) = '\0';
		puts(line[cl]);
	}

	sort_line(line, 0, cl - 1);

	fclose(fp);
	for(int i = 0; line[i] != NULL; ++i){
		//puts(line[i]);
		free(line[i]);
	}
}

void sort_line(char *line[], int left, int right){
	int i = left;
	int j = right;
	char *pivot = line[left + (right - left) / 2];

	for(;;){
		while(strcmp(line[i], pivot) < 0)
			i++;
		while(strcmp(line[j], pivot) > 0)
			j--;
		if(i >= j){
			break;
		}

		swap(line, i, j);
		i++;
		j--;
	}

	if(left < i - 1){
		sort_line(line, left, i - 1);
	}
	if(j + 1 < right){
		sort_line(line, j + 1, right);
	}
}

void swap(char *line[], int i, int j){
	char *tmp = line[i];
	line[i] = line[j];
	line[j] = tmp;
}

void clean(void){
	DIRSTACK *tmp1;
	ALIAS *tmp2;

	while(NULL != dirhead){
		tmp1 = dirhead->next;
		free(dirhead);
		dirhead = tmp1;
	}

	for(int i = 0; i < HISTORY_SIZE; ++i){
		free(command_history[i]);
	}

	while(NULL != aliashead){
		tmp2 = aliashead->next;
		free(aliashead);
		aliashead = tmp2;
	}
}

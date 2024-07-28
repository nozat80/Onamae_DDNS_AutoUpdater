#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>

#define BUFFER_SIZE 1024

const char* USER_ID = "xxxxxxxx";
const char* PASS = "Password";
const char* HOSTNAME = "hostname";
const char* DOMNAME = "example.com";



char ip_file[PATH_MAX];

int main(int argc, char *argv[]) {
    if (argc == 0 || argv[0] == NULL) {
        printf("ERROR!! Can't get executable path\n");
        return -1;
    }

    // 実行ファイルのパスを取得
    char exe_path[PATH_MAX];
    if (realpath(argv[0], exe_path) == NULL) {
        printf("ERROR!! Can't resolve executable path\n");
        return -1;
    }

    // 実行ファイルのディレクトリを取得
    char *exe_dir = dirname(exe_path);

    // 実行ファイルのディレクトリに IP_FILE のパスを設定
    snprintf(ip_file, sizeof(ip_file), "%s/current_ip.txt", exe_dir);


    FILE *file;
    char buffer[128];
    char previous_ip[128] = "";
    char response[BUFFER_SIZE];
    int to_child_pipe[2];
    int from_child_pipe[2];
    pid_t pid;


    // 現在のIPアドレスを取得
    file = popen("curl ifconfig.io -4", "r");
    if (file == NULL) {
        printf("ERROR!! Can't reach \"ifconfig.io\"\n");
        return -1;
    }
    fgets(buffer, 128, file);
    pclose(file);

    // 前回登録したIPアドレスを読み込み
    file = fopen(ip_file, "r");
    if (file != NULL) {
        fgets(previous_ip, 128, file);
        fclose(file);
    }

    // IPアドレスが変更されたかを確認
    if (strcmp(buffer, previous_ip) != 0) {
        printf("IPアドレスが変更されました: %s\n", buffer);

        // Create pipes
        if (pipe(to_child_pipe) == -1 || pipe(from_child_pipe) == -1) {
            perror("pipe");
            return 1;
        }

        // Fork a child process
        pid = fork();
        if (pid == -1) {
            perror("fork");
            return 1;
        } else if (pid == 0) { // Child process
            // Redirect stdin and stdout
            dup2(to_child_pipe[0], STDIN_FILENO);
            dup2(from_child_pipe[1], STDOUT_FILENO);

            // Close unused pipe ends
            close(to_child_pipe[1]);
            close(from_child_pipe[0]);

            // Execute openssl s_client
            execlp("openssl", "openssl", "s_client", "-connect", "ddnsclient.onamae.com:65010", "-quiet", (char *)NULL);
            perror("execlp");
            exit(1);
        } else { // Parent process
            // Close unused pipe ends
            close(to_child_pipe[0]);
            close(from_child_pipe[1]);

            FILE *to_child = fdopen(to_child_pipe[1], "w");
            FILE *from_child = fdopen(from_child_pipe[0], "r");

            if (to_child == NULL || from_child == NULL) {
                perror("fdopen");
                return 1;
            }

            while (fgets(response, BUFFER_SIZE, from_child) != NULL) {
                if (strstr(response,".") != NULL){
                    break;
                }
                printf("CONNECT Response: %s", response);
                if (strstr(response, "SUCCESS") == NULL) {
                    fprintf(stderr, "Connect failed\n");
                    fclose(to_child);
                    fclose(from_child);
                    wait(NULL);
                    return 1;
                }
                
            }

            // Send LOGIN command
            fprintf(to_child, "LOGIN\nUSERID:%s\nPASSWORD:%s\n.\n", USER_ID, PASS);
            fflush(to_child);
            sleep(1);

            // Read server response for LOGIN
            while (fgets(response, BUFFER_SIZE, from_child) != NULL) {
                if (strstr(response,".") != NULL){
                    break;
                }
                printf("LOGIN Response: %s", response);
                if (strstr(response, "SUCCESS") == NULL) {
                    fprintf(stderr, "Login failed\n");
                    fclose(to_child);
                    fclose(from_child);
                    wait(NULL);
                    return 1;
                }
                
            }

            // Send MODIP command
            fprintf(to_child, "MODIP\nHOSTNAME:%s\nDOMNAME:%s\nIPV4:%s\n.\n", HOSTNAME, DOMNAME, buffer);
            fflush(to_child);
            sleep(1);

            // Read server response for MODIP
            while (fgets(response, BUFFER_SIZE, from_child) != NULL) {
                if (strstr(response,".") != NULL){
                    break;
                }
                printf("MODIP Response: %s", response);
                if (strstr(response, "SUCCESS") == NULL) {
                    fprintf(stderr, "MODIP command failed\n");
                    fclose(to_child);
                    fclose(from_child);
                    wait(NULL);
                    return 1;
                }
                
            }

            // Send LOGOUT command
            fprintf(to_child, "LOGOUT\n.\n");
            fflush(to_child);
            sleep(1);

            // Read server response for LOGOUT
            if (fgets(response, BUFFER_SIZE, from_child) != NULL) {
                printf("LOGOUT Response: %s", response);
            }

            fclose(to_child);
            fclose(from_child);
            wait(NULL);
         
        }

        // 現在のIPアドレスをファイルに保存
        file = fopen(ip_file, "w");
        if (file != NULL) {
            fprintf(file, "%s\n", buffer);
            fclose(file);
        } else {
            printf("ERROR!! Can't write to \"%s\"\n", ip_file);
        }
    } else {
        printf("IPアドレスに変更はありません: %s\n", buffer);
    }

    printf("正常終了\n");
    return 0;
}

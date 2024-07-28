#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>

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

        // DNSの登録を実行
        file = popen("openssl s_client -connect ddnsclient.onamae.com:65010 -quiet", "w");
        if (file == NULL) {
            perror("popen");
            return 1;
        }
        
        fprintf(file, "LOGIN\nUSERID:%s\nPASSWORD:%s\n.\n", USER_ID, PASS);
        fflush(file);
        sleep(1);
        if (fgets(response, BUFFER_SIZE, file) != NULL) {
            printf("LOGIN Response: %s", response);
            if (strstr(response, "SUCCESS") == NULL) {
                fprintf(stderr, "Login failed\n");
                pclose(file);
                return 1;
            }
        }
        
        fprintf(file, "MODIP\nHOSTNAME:%s\nDOMNAME:%s\nIPV4:%s\n.\n", HOSTNAME, DOMNAME, buffer);
        fflush(file);
        sleep(1);        
        if (fgets(response, BUFFER_SIZE, file) != NULL) {
            printf("MODIP Response: %s", response);
            if (strstr(response, "SUCCESS") == NULL) {
                fprintf(stderr, "MODIP command failed\n");
                pclose(file);
                return 1;
            }
        }

        fprintf(file, "LOGOUT\n.\n");
        fflush(file);
        sleep(1);
        // Read server response for LOGOUT
        if (fgets(response, BUFFER_SIZE, file) != NULL) {
            printf("LOGOUT Response: %s", response);
        }

        pclose(file);

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

    printf("正常終了");
    return 0;
}

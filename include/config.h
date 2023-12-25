#define NAME config[0]
#define C_NAME config[1]
#define LAT config[2]
#define LON config[3]
#define WORK_M config[4]
#define DHCP config[5]
#define AP config[6]
#define IP config[7]
#define GATEWAY config[8]
#define SUBNET config[9]
#define DNS01 config[10]
#define DNS02 config[11]
#define _SSID config[12]
#define _PASSWD config[13]
#define _PASSWD_AP config[14]
#define STANS_TYPE config[15]
#define SERVER config[16]
#define PORT config[17]
#define AUTH config[18]
#define USER_S config[19]
#define PASSWD_S config[20]
#define TOPIC config[21]
#define RANGE config[22]
#define UUID config[23]
#define DESCRIPTION config[24]

#define IS_DHCP (DHCP[0] == '1')
#define IS_AP (AP[0] == '0')
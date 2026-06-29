#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pcap.h>          // PCAP 라이브러리
#include <arpa/inet.h>     // inet_ntoa, ntohs 등 네트워크 함수

/* Ethernet 헤더 구조체 - 패킷 맨 앞 (14바이트) */
struct ethheader {
    u_char  ether_dhost[6];  // 목적지 MAC 주소 (6바이트)
    u_char  ether_shost[6];  // 출발지 MAC 주소 (6바이트)
    u_short ether_type;      // 상위 프로토콜 타입
};

/* IP 헤더 구조체 */
struct ipheader {
    unsigned char      iph_ihl:4,       // IP 헤더 길이
                       iph_ver:4;       // IP 버전 (IPv4=4)
    unsigned char      iph_tos;         // 서비스 타입
    unsigned short int iph_len;         // IP 패킷 전체 길이 (헤더 + 데이터)
    unsigned short int iph_ident;       // 단편화 식별자
    unsigned short int iph_flag:3,      // 단편화 플래그
                       iph_offset:13;   // 단편화 오프셋
    unsigned char      iph_ttl;         // TTL (패킷 수명)
    unsigned char      iph_protocol;    // 상위 프로토콜 (TCP=6)
    unsigned short int iph_chksum;      // IP 헤더 체크섬
    struct in_addr     iph_sourceip;    // 출발지 IP 주소
    struct in_addr     iph_destip;      // 목적지 IP 주소
};

/* TCP 헤더 구조체 */
struct tcpheader {
    u_short tcp_sport;   // 출발지 포트 번호
    u_short tcp_dport;   // 목적지 포트 번호
    u_int   tcp_seq;     // 시퀀스 번호
    u_int   tcp_ack;     // ACK 번호
    u_char  tcp_offx2;   // 상위 4비트: TCP 헤더 길이
/* tcp_offx2 상위 4비트 추출 -> TCP 헤더 길이 구함 */
#define TH_OFF(th) (((th)->tcp_offx2 & 0xf0) >> 4)
    u_char  tcp_flags;   // TCP 플래그
    u_short tcp_win;     // 윈도우 크기
    u_short tcp_sum;     // TCP 체크섬
    u_short tcp_urp;     // 긴급 포인터
};

/* 패킷 캡처 시 자동 호출 */
void got_packet(u_char *args, const struct pcap_pkthdr *header,
                              const u_char *packet)
{
    /* 패킷 시작 주소 -> Ethernet 헤더 구조체로 타입캐스팅 */
    struct ethheader *eth = (struct ethheader *)packet;

    /* Ethernet 타입이 IP(0x0800)가 아니면 무시 */
    if (ntohs(eth->ether_type) != 0x0800) return;

    /* Ethernet 헤더 크기만큼 건너뛴 위치부터 IP 헤더 시작 */
    struct ipheader *ip = (struct ipheader *)(packet + sizeof(struct ethheader));

    /* TCP 프로토콜이 아니면 무시 */
    if (ip->iph_protocol != IPPROTO_TCP) return;

    /* IP 헤더 실제 길이 계산 : iph_ihl은 4바이트 단위 -> *4 */
    int ip_header_len = ip->iph_ihl * 4;

    /* IP 헤더 시작 주소에서 ip_header_len만큼 건너뛴 위치부터 TCP 헤더 시작 */
    struct tcpheader *tcp = (struct tcpheader *)((u_char *)ip + ip_header_len);

    /* TCP 헤더 실제 길이 계산 : TH_OFF 매크로로 상위 4비트 추출 후 *4 */
    int tcp_header_len = TH_OFF(tcp) * 4;

    /* TCP 헤더 이후 :  Application 계층 데이터(HTTP Message) */
    u_char *payload = (u_char *)tcp + tcp_header_len;

    /* payload 길이 = IP 전체 길이 - IP 헤더 길이 - TCP 헤더 길이 */
    int payload_len = ntohs(ip->iph_len) - ip_header_len - tcp_header_len;

    printf("===========================================\n");

    /* Ethernet 헤더 정보 출력 */
    printf("[Ethernet Header]\n");
    printf("  Src MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           eth->ether_shost[0], eth->ether_shost[1], eth->ether_shost[2],
           eth->ether_shost[3], eth->ether_shost[4], eth->ether_shost[5]);
    printf("  Dst MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           eth->ether_dhost[0], eth->ether_dhost[1], eth->ether_dhost[2],
           eth->ether_dhost[3], eth->ether_dhost[4], eth->ether_dhost[5]);

    /* IP 헤더 정보 출력 - inet_ntoa로 IP 주소를 문자열 변환 */
    printf("[IP Header]\n");
    printf("  Src IP: %s\n", inet_ntoa(ip->iph_sourceip));
    printf("  Dst IP: %s\n", inet_ntoa(ip->iph_destip));

    /* TCP 헤더 정보 출력 - ntohs로 네트워크 바이트 순서를 호스트 순서로 변환 */
    printf("[TCP Header]\n");
    printf("  Src Port: %d\n", ntohs(tcp->tcp_sport));
    printf("  Dst Port: %d\n", ntohs(tcp->tcp_dport));

    /* HTTP Message 출력 */    
    if (payload_len > 0) {
        printf("[HTTP Message]\n");
        printf("  Length: %d bytes\n", payload_len);
        printf("  Content: ");
        /* 출력 가능한 ASCII 문자만 출력 (최대 500바이트) */
        for (int i = 0; i < payload_len && i < 500; i++) {
            if (payload[i] == '\r') continue;                 
            if (payload[i] == '\n') printf("\n           ");     
            else if (payload[i] >= 32 && payload[i] < 127)      
                printf("%c", payload[i]);
        }
        printf("\n");
    }

    printf("\n");
}

/* main 함수 */
int main()
{
    pcap_t *handle;                      
    char errbuf[PCAP_ERRBUF_SIZE];       
    struct bpf_program fp;               
    char filter_exp[] = "tcp";           // 필터 표현식: TCP 패킷만 캡처
    bpf_u_int32 net = 0;                 // 네트워크 주소

    /* 1) 네트워크 인터페이스 열기 */
    handle = pcap_open_live("eth0",  // 캡처할 인터페이스 (WSL: eth0)
                            BUFSIZ,  // 캡처할 패킷 최대 크기
                            1,       // 모든 패킷 캡처
                            1000,    // 타임아웃 (ms)
                            errbuf); // 에러 메시지 저장
    if (handle == NULL) {
        fprintf(stderr, "pcap_open_live 실패: %s\n", errbuf);
        return 1;
    }

    /* 2) 필터 표현식을 BPF 의사코드로 컴파일 */
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "필터 컴파일 실패: %s\n", pcap_geterr(handle));
        return 1;
    }

    /* 3) 컴파일된 필터 적용 - TCP 패킷만 */
    if (pcap_setfilter(handle, &fp) != 0) {
        fprintf(stderr, "필터 설정 실패: %s\n", pcap_geterr(handle));
        return 1;
    }

    printf("TCP 패킷 캡처 시작... (Ctrl+C로 종료)\n\n");

    /* 4) 패킷 캡처 루프 시작 */
    pcap_loop(handle, -1, got_packet, NULL);

    pcap_close(handle);
    return 0;
}

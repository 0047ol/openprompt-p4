/* Generated from shared/ui/connection-guide.json. */
#pragma once
#define TP_GUIDE_LANDSCAPE_HEIGHT 482
#define TP_GUIDE_PORTRAIT_HEIGHT 818
#define TP_GUIDE_CONTENT_WIDTH 1000
#define TP_GUIDE_COLUMN_GAP 52
#define TP_GUIDE_TITLE_GAP 28
#define TP_GUIDE_PARAGRAPH_GAP 28
#define TP_GUIDE_BODY_LINE_HEIGHT 36
#define TP_GUIDE_TITLE_LINE_HEIGHT 48
#define TP_GUIDE_QR_SIZE 188
#define TP_GUIDE_QR_PADDING 10
#define TP_GUIDE_STEP_COUNT 3
static const char * const tp_guide_steps[TP_GUIDE_STEP_COUNT][2] = {
{"连接网络","Network"},
{"打开后台","Open controller"},
{"开始使用","Get started"},
};
static const tp_guide_copy_t tp_guide_copy[] = {
[TP_GUIDE_JOIN]={1,{"连接网络","Connect to the network"},{"请使用摄像头扫描左侧二维码连接设备热点。\n若无法扫描，可手动搜索设备的热点名称：{hotspot}。","Scan the QR code with your camera to join the device hotspot.\nIf scanning is unavailable, find the hotspot named {hotspot}."},true,false,false},
[TP_GUIDE_WIFI_CONNECTING]={1,{"正在连接 Wi-Fi 中...","Connecting to Wi-Fi..."},{"",""},false,true,true},
[TP_GUIDE_LAN]={2,{"打开后台","Open controller"},{"请先连接 Wi-Fi：{ssid}，同一网络下\n扫码或打开 {url} 开始使用。","Join Wi-Fi: {ssid}. On the same network,\nscan the QR code or open {url} to get started."},true,false,false},
[TP_GUIDE_BLUETOOTH]={2,{"打开后台","Open controller"},{"请先扫码或打开：{url}\n连接蓝牙（{bluetooth}）开始使用。","Scan the QR code or open {url}.\nConnect to Bluetooth ({bluetooth}) to get started."},true,false,false},
[TP_GUIDE_RECONNECT]={3,{"配置已完成","Setup complete"},{"请先连接 Wi-Fi：{ssid}，同一网络下\n扫码或打开 {url} 开始使用。","Join Wi-Fi: {ssid}. On the same network,\nscan the QR code or open {url} to get started."},true,false,false},
[TP_GUIDE_READY]={3,{"配置已完成","Setup complete"},{"请先连接 Wi-Fi：{ssid}，同一网络下\n扫码或打开 {url} 开始使用。","Join Wi-Fi: {ssid}. On the same network,\nscan the QR code or open {url} to get started."},true,false,false},
};

#include "CTsPlayer.h"
#include "Util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <android/native_window.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include "player_set_sys.h"
#include "Amsysfsutils.h"
#include <sys/times.h>
#include <time.h>
#include <sys/ioctl.h>
#include <media/AudioSystem.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <media/IMediaPlayerService.h>
#include "subtitleservice.h"
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayInfo.h>
#include <gui/Surface.h>

#ifdef USE_OPTEEOS
#include <PA_Decrypt.h>
#endif

using namespace android;

#define M_LIVE	1
#define M_TVOD	2
#define M_VOD	3
#define RES_VIDEO_SIZE 256
#define RES_AUDIO_SIZE 64
#define MAX_WRITE_COUNT 20

#define MAX_WRITE_ALEVEL 0.99
#define MAX_WRITE_VLEVEL 0.99
#define READ_SIZE (64 * 1024)
#define CTC_BUFFER_LOOP_NSIZE 1316

static bool m_StopThread = false;

//log switch
static int prop_shouldshowlog = 0;
static int prop_playerwatchdog_support =0;
static bool s_h264sameucode = false;
int prop_softdemux = 0;
int prop_esdata = 0;
int prop_multi_play = 0;
int prop_dumpfile = 0;
int prop_buffertime = 0;
int prop_readffmpeg = 0;
int hasaudio = 0;
int hasvideo = 0;
int prop_softfit = 0;
int prop_blackout_policy = 1;
float prop_audiobuflevel = 0.0;
float prop_videobuflevel = 0.0;
int prop_audiobuftime = 1000;
int prop_videobuftime = 1000;
int prop_show_first_frame_nosync = 0;
int keep_vdec_mem = 0;
int prop_write_log = 0;
int prop_trickmode_debug = 0;

static int vdec_underflow = 0;
static int adec_underflow = 0;

int checkcount = 0;
int checkcount1 = 0;
int buffersize = 0;
char old_free_scale_axis[64] = {0};
char old_window_axis[64] = {0};
char old_free_scale[64] = {0};
int s_video_axis[4] = {0};
static LPBUFFER_T lpbuffer_st;
static int H264_error_skip_normal = 0;
static int H264_error_skip_ff = 0;
static int H264_error_skip_reserve = 0;
static unsigned int prev_aread = 0;
static unsigned int prev_vread = 0;
static int arp_is_changed = 0;
static int vrp_is_changed = 0;
static int prop_start_no_out = 0;
//unsigned int am_sysinfo_param =0x08;

/* soft demux related*/
#define _GNU_SOURCE
#define F_SETPIPE_SZ        (F_LINUX_SPECIFIC_BASE + 7)
#define F_GETPIPE_SZ        (F_LINUX_SPECIFIC_BASE + 8)
#include <fcntl.h>
#include "amffextractor.h"

static int s_nDumpTs = 0;
static int pipe_fd[2] = { -1, -1 };
static bool am_ffextractor_inited = false;
static int read_cb(void *opaque, uint8_t *buf, int size) {
    int ret = read(pipe_fd[0], buf, size);

    return ret;
}

#define LOGV(...) \
    do { \
        if (prop_shouldshowlog) { \
            __android_log_print(ANDROID_LOG_VERBOSE, "TsPlayer", __VA_ARGS__); \
        } \
    } while (0)

#define LOGD(...) \
    do { \
        if (prop_shouldshowlog) { \
            __android_log_print(ANDROID_LOG_DEBUG, "TsPlayer", __VA_ARGS__); \
        } \
    } while (0)

/*
#define LOGI(...) \
    do { \
        if (prop_shouldshowlog) { \
            __android_log_print(ANDROID_LOG_INFO, "TsPlayer", __VA_ARGS__); \
        } \
    } while (0)

*/
//#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "TsPlayer", __VA_ARGS__)
//#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "TsPlayer", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO  , "TsPlayer", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN  , "TsPlayer", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , "TsPlayer", __VA_ARGS__)
#define SCALING_MODE  "1"
#define DEFAULT_MODE  "0"
#define CPU_SCALING_MODE_NODE  "/sys/devices/system/cpu/cpufreq/interactive/boost"
int perform_flag =0;

#ifdef TELECOM_VFORMAT_SUPPORT
/*
//telecom video format define
typedef enum {
    VFORMAT_UNKNOWN = -1,
    VFORMAT_MPEG12 = 0,
    VFORMAT_MPEG4 = 1,
    VFORMAT_H264 = 2,
    VFORMAT_MJPEG = 3,
    VFORMAT_REAL = 4,
    VFORMAT_JPEG = 5,
    VFORMAT_VC1 = 6,
    VFORMAT_AVS = 7,
    VFORMAT_H265 = 8,
    VFORMAT_SW = 9,
    VFORMAT_UNSUPPORT,
    VFORMAT_MAX
} vformat_t;
*/
#define CT_VFORMAT_H265 8
#define CT_VFORMAT_SW 9
#define CT_VFORMAT_UNSUPPORT 10
/*
//telecom audio format define
typedef enum {
    AFORMAT_UNKNOWN2 = -2,
    AFORMAT_UNKNOWN = -1,
    AFORMAT_MPEG   = 0,
    AFORMAT_PCM_S16LE = 1,
    AFORMAT_AAC   = 2,
    AFORMAT_AC3   = 3,
    AFORMAT_ALAW = 4,
    AFORMAT_MULAW = 5,
    AFORMAT_DTS = 6,
    AFORMAT_PCM_S16BE = 7,
    AFORMAT_FLAC = 8,
    AFORMAT_COOK = 9,
    AFORMAT_PCM_U8 = 10,
    AFORMAT_ADPCM = 11,
    AFORMAT_AMR  = 12,
    AFORMAT_RAAC  = 13,
    AFORMAT_WMA  = 14,
    AFORMAT_WMAPRO   = 15,
    AFORMAT_PCM_BLURAY  = 16,
    AFORMAT_ALAC  = 17,
    AFORMAT_VORBIS    = 18,
    AFORMAT_DDPlUS = 19,
    AFORMAT_UNSUPPORT,
    AFORMAT_MAX
} aformat_t;
*/
#define CT_AFORMAT_UNKNOWN2 -2
#define CT_AFORMAT_DDPlUS 19
#define CT_AFORMAT_UNSUPPORT 20

vformat_t changeVformat(vformat_t index)
{
    LOGI("changeVformat, vfromat: %d\n", index);
    if(index == CT_VFORMAT_H265)
        return VFORMAT_HEVC;
    else if(index == CT_VFORMAT_SW)
        return VFORMAT_SW;

    if(index >= CT_VFORMAT_UNSUPPORT)
        return VFORMAT_UNSUPPORT;
    else
        return index;
}

aformat_t changeAformat(aformat_t index)
{
    LOGI("changeAformat, afromat: %d\n", index);
    if(index == CT_AFORMAT_UNKNOWN2)
        return AFORMAT_UNKNOWN;
    else if(index == CT_AFORMAT_DDPlUS)
        return AFORMAT_EAC3;

    if(index >= CT_AFORMAT_UNSUPPORT)
        return AFORMAT_UNSUPPORT;
    else
        return index;
}
#endif

void InitOsdScale(int width, int height)
{
    LOGI("InitOsdScale, width: %d, height: %d\n", width, height);
    int x = 0, y = 0, w = 0, h = 0;
    char fsa_bcmd[64] = {0};
    char wa_bcmd[64] = {0};

    sprintf(fsa_bcmd, "0 0 %d %d", width-1, height-1);
    LOGI("InitOsdScale, free_scale_axis: %s\n", fsa_bcmd);
    OUTPUT_MODE output_mode = get_display_mode();
    getPosition(output_mode, &x, &y, &w, &h);
    sprintf(wa_bcmd, "%d %d %d %d", x, y, x+w-1, y+h-1);
    LOGI("InitOsdScale, window_axis: %s\n", wa_bcmd);

    amsysfs_set_sysfs_int("/sys/class/graphics/fb0/blank", 1);
    amsysfs_set_sysfs_str("/sys/class/graphics/fb0/freescale_mode", "1");
    amsysfs_set_sysfs_str("/sys/class/graphics/fb0/free_scale_axis", fsa_bcmd);
    amsysfs_set_sysfs_str("/sys/class/graphics/fb0/window_axis", wa_bcmd);
    amsysfs_set_sysfs_str("/sys/class/graphics/fb0/free_scale", "0x10001");
    amsysfs_set_sysfs_int("/sys/class/graphics/fb0/blank", 0);
}

void reinitOsdScale()
{
    LOGI("reinitOsdScale, old_free_scale_axis: %s\n", old_free_scale_axis);
    LOGI("reinitOsdScale, old_window_axis: %s\n", old_window_axis);
    LOGI("reinitOsdScale, old_free_scale: %s\n", old_free_scale);
    amsysfs_set_sysfs_int("/sys/class/graphics/fb0/blank", 1);
    amsysfs_set_sysfs_str("/sys/class/graphics/fb0/freescale_mode", "1");
    amsysfs_set_sysfs_str("/sys/class/graphics/fb0/free_scale_axis", old_free_scale_axis);
    amsysfs_set_sysfs_str("/sys/class/graphics/fb0/window_axis", old_window_axis);
    if(!strncmp(old_free_scale, "free_scale_enable:[0x1]", 23)) {
        amsysfs_set_sysfs_str("/sys/class/graphics/fb0/free_scale", "0x10001");
    }
    else {
        amsysfs_set_sysfs_str("/sys/class/graphics/fb0/free_scale", "0x0");
    }
    amsysfs_set_sysfs_int("/sys/class/graphics/fb0/blank", 0);
}

void LunchIptv(bool isSoftFit)
{
    LOGI("LunchIptv isSoftFit:%d\n", isSoftFit);
    char value[PROPERTY_VALUE_MAX] = {0};

    property_get("init.svc.bootvideo", value, "");
    if(!isSoftFit) {
        //amsysfs_set_sysfs_str("/sys/class/graphics/fb0/video_hole", "0 0 1280 720 0 8");
        amsysfs_set_sysfs_str("/sys/class/deinterlace/di0/config", "disable");
        amsysfs_set_sysfs_int("/sys/module/di/parameters/buf_mgr_mode", 0);
    }else {
        if(strncmp(value, "running", 7) != 0)
            amsysfs_set_sysfs_int("/sys/class/graphics/fb0/blank", 0);
    }
}

void QuitIptv(bool isSoftFit, bool isBlackoutPolicy)
{
    amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_hd", 0);
    //amsysfs_set_sysfs_str("/sys/class/graphics/fb0/video_hole", "0 0 0 0 0 0");
    if(!isBlackoutPolicy)
        amsysfs_set_sysfs_int("/sys/class/video/blackout_policy", 1);
    else {
        if(amsysfs_get_sysfs_int("/sys/class/video/disable_video") != 2)
            amsysfs_set_sysfs_int("/sys/class/video/disable_video", 2);
    }

    if(!isSoftFit) {
        reinitOsdScale();
    } else {
        amsysfs_set_sysfs_int("/sys/class/graphics/fb0/blank", 0);
    }
    LOGI("QuitIptv\n");
}

int64_t av_gettime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
int sysfs_get_long(char *path, unsigned long  *val)
{
    char buf[64];

    if (amsysfs_get_sysfs_str(path, buf, sizeof(buf)) == -1) {
        LOGI("unable to open file %s,err: %s", path, strerror(errno));
        return -1;
    }
    if (sscanf(buf, "0x%lx", val) < 1) {
        LOGI("unable to get pts from: %s", buf);
        return -1;
    }
    return 0;
}
void test_player_evt_func(IPTV_PLAYER_EVT_e evt, void *handler)
{
    switch (evt) {
	case IPTV_PLAYER_EVT_STREAM_VALID:
	case IPTV_PLAYER_EVT_FIRST_PTS:
	case IPTV_PLAYER_EVT_VOD_EOS:
	case IPTV_PLAYER_EVT_ABEND:
	case IPTV_PLAYER_EVT_PLAYBACK_ERROR:
	case IPTV_PLAYER_EVT_VID_FRAME_ERROR:
	case IPTV_PLAYER_EVT_VID_DISCARD_FRAME:
	case IPTV_PLAYER_EVT_VID_DEC_UNDERFLOW:
	case IPTV_PLAYER_EVT_VID_PTS_ERROR:
	case IPTV_PLAYER_EVT_AUD_FRAME_ERROR:
	case IPTV_PLAYER_EVT_AUD_DISCARD_FRAME:
	case IPTV_PLAYER_EVT_AUD_DEC_UNDERFLOW:
	case IPTV_PLAYER_EVT_AUD_PTS_ERROR:
	case IPTV_PLAYER_EVT_BUTT:
    default :
	    LOGV("evt : %d\n",evt);
		break;
    }
}


#ifdef USE_OPTEEOS
CTsPlayer::CTsPlayer()
{
    CTsPlayer(false);
}

CTsPlayer::CTsPlayer(bool DRMMode)
#else
CTsPlayer::CTsPlayer()
#endif
{
    char value[PROPERTY_VALUE_MAX] = {0};

    property_get("iptv.shouldshowlog", value, "0");//initial the log switch
    prop_shouldshowlog = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.readffmpeg.time", value, "5");
    prop_readffmpeg = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.dumpfile", value, "0");
    prop_dumpfile = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.softdemux", value, "0");
    prop_softdemux = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.middle.softdemux", value, "0");
    prop_esdata = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
	if (property_get("media.ctcplayer.enable", value, NULL) > 0)
        prop_multi_play = atoi(value);

#ifdef USE_OPTEEOS
    if(DRMMode)
        prop_softdemux = 1;
#endif

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.buffer.time", value, "2300");
    prop_buffertime = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.audio.bufferlevel", value, "0.6");
    prop_audiobuflevel = atof(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.video.bufferlevel", value, "0.8");
    prop_videobuflevel = atof(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.audio.buffertime", value, "1000");
    prop_audiobuftime = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.video.buffertime", value, "1000");
    prop_videobuftime = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.show_first_frame_nosync", value, "1");
    prop_show_first_frame_nosync = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.softfit", value, "1");
    prop_softfit = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.blackout.policy", value, "1");
    prop_blackout_policy = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.buffersize", value, "5000");
    buffersize = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.playerwatchdog.support", value, "0");
    prop_playerwatchdog_support = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.h264.error_skip_normal", value, "0");
    H264_error_skip_normal = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.h264.error_skip_ff", value, "1");
    H264_error_skip_ff = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.h264.error_skip_reserve", value, "20");
    H264_error_skip_reserve = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.start.no.out", value, "0");
    prop_start_no_out = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.write.log", value, "0");
    prop_write_log = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.trickmode.debug", value, "0");
    prop_trickmode_debug = atoi(value);

    memset(value, 0, PROPERTY_VALUE_MAX);
    amsysfs_get_sysfs_str("/sys/class/cputype/cputype", value, PROPERTY_VALUE_MAX);
    LOGI("/sys/class/cputype/cputype:%s\n", value);
    if (value[0] != '\0' && (!strcasecmp(value, "905L") || !strcasecmp(value, "905M2")))
        s_h264sameucode = true;

    LOGI("CTsPlayer, prop_shouldshowlog: %d, prop_buffertime: %d, prop_dumpfile: %d, audio bufferlevel: %f,video bufferlevel: %f, prop_softfit: %d,player_watchdog_support:%d, isDrm: %d prop_write_log:%d\n",
		        prop_shouldshowlog, prop_buffertime, prop_dumpfile, prop_audiobuflevel, prop_videobuflevel, prop_softfit,prop_playerwatchdog_support, prop_softdemux, prop_write_log);
    LOGI("iptv.audio.buffertime = %d, iptv.video.buffertime = %d prop_start_no_out:%d, prop_trickmode_debug=%d\n", prop_audiobuftime, prop_videobuftime,prop_start_no_out, prop_trickmode_debug);

    char buf[64] = {0};
    memset(old_free_scale_axis, 0, 64);
    memset(old_window_axis, 0, 64);
    memset(old_free_scale, 0, 64);
    amsysfs_get_sysfs_str("/sys/class/graphics/fb0/free_scale_axis", old_free_scale_axis, 64);
    amsysfs_get_sysfs_str("/sys/class/graphics/fb0/window_axis", buf, 64);
    amsysfs_get_sysfs_str("/sys/class/graphics/fb0/free_scale", old_free_scale, 64);

    LOGI("window_axis: %s\n", buf);
    char *pr = strstr(buf, "[");
    if(pr != NULL) {
        int len = strlen(pr);
        int i = 0, j = 0;
        for(i=1; i<len-1; i++) {
            old_window_axis[j++] = pr[i];
        }
        old_window_axis[j] = 0;
    }

    LOGI("free_scale_axis: %s\n", old_free_scale_axis);
    LOGI("window_axis: %s\n", old_window_axis);
    LOGI("free_scale: %s\n", old_free_scale);

    if(amsysfs_get_sysfs_int("/sys/class/video/disable_video") == 1)
        amsysfs_set_sysfs_int("/sys/class/video/disable_video", 2);
    memset(a_aPara, 0, sizeof(AUDIO_PARA_T)*MAX_AUDIO_PARAM_SIZE);
    memset(sPara, 0, sizeof(SUBTITLE_PARA_T)*MAX_SUBTITLE_PARAM_SIZE);
    memset(&vPara, 0, sizeof(vPara));
    memset(&codec, 0, sizeof(codec));
    player_pid = -1;
    pcodec = &codec;
    if(prop_softdemux != 0){
        vcodec = (codec_para_t *)malloc(sizeof(codec_para_t));
        if(vcodec == NULL){
            LOGI("vcodec alloc fail\n");
        }
       memset(vcodec, 0, sizeof(codec_para_t));

       acodec = (codec_para_t *)malloc(sizeof(codec_para_t));
       if(acodec == NULL){
           LOGI("acodec alloc fail\n");
       }
       memset(acodec, 0, sizeof(codec_para_t));
	   LOGI("prop_readffmpeg = %d\n",prop_readffmpeg);
    }
    codec_audio_basic_init();
    lp_lock_init(&mutex, NULL);
    //0:normal，1:full stretch，2:4-3，3:16-9
    int screen_mode = 0;
    int project_type = 0;
    property_get("sys.proj.type",value,NULL);
    if(!strcmp(value,"telecom")){
        property_get("sys.proj.tender.type",value,NULL);
        if(!strcmp(value,"jicai")){
            project_type=1;
        }
    }
    property_get("ubootenv.var.screenmode",value,"full");
    if(!strcmp(value,"normal")){
        if(project_type==1)
            screen_mode = 1;
        else
            screen_mode = 0;
    }
    else if(!strcmp(value,"full"))
         screen_mode = 1;
    else if(!strcmp(value,"4_3"))
         screen_mode = 2;
    else if(!strcmp(value,"16_9"))
         screen_mode = 3;
    else if(!strcmp(value,"4_3 letter box"))
        screen_mode = 7;
    else if(!strcmp(value,"16_9 letter box"))
        screen_mode = 11;
    else
        screen_mode = 1;


    amsysfs_set_sysfs_int("/sys/class/video/screen_mode", screen_mode);
    amsysfs_set_sysfs_int("/sys/class/tsync/enable", 1);

    //set overflow status when decode h264_4k use format h264 .
    amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/fatal_error_reset", 1);

    m_bIsPlay = false;
    m_bIsPause = false;
    pfunc_player_evt = test_player_evt_func;
    m_nOsdBpp = 16;//SYS_get_osdbpp();
    m_nAudioBalance = 3;

    m_nVolume = 100;
    m_bFast = false;
    m_bSetEPGSize = false;
    m_bWrFirstPkg = true;
    m_StartPlayTimePoint = 0;
    m_PreviousOverflowTime = 0;
    m_isSoftFit = (prop_softfit == 1) ? true : false;
    m_isBlackoutPolicy = (prop_blackout_policy == 1) ? true : false;
    m_StopThread = false;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&mThread, &attr, threadCheckAbend, this);
    pthread_attr_destroy(&attr);

    pfunc_player_param_evt = NULL;
    player_evt_param_handler = NULL;
    pthread_attr_init(&attr);
    pthread_create(&mInfoThread, &attr, threadReportInfo, this);
    pthread_attr_destroy(&attr);


    if(prop_softdemux == 1 && prop_esdata != 1){
        pthread_attr_init(&attr);
        pthread_create(&readThread, &attr, threadReadPacket, this);
        pthread_attr_destroy(&attr);
    }

    //m_nMode = M_LIVE;
    LunchIptv(m_isSoftFit);
    m_fp = NULL;
    sp<IBinder> binder =defaultServiceManager()->getService(String16("media.player"));
    sp<IMediaPlayerService> service = interface_cast<IMediaPlayerService>(binder);
    if(service.get() != NULL){
			  LOGI("CTsPlayer stopPlayerIfNeed \n");
			  service->stopPlayerIfNeed();
		  	LOGI("CTsPlayer stopPlayerIfNeed ==end\n");
    }
    mIsOmxPlayer = false;
    memset(&m_sCtsplayerState, 0, sizeof(struct ctsplayer_state));
}

#define AML_VFM_MAP "/sys/class/vfm/map"
static int add_di()
{
    amsysfs_set_sysfs_str(AML_VFM_MAP, "rm default");
    amsysfs_set_sysfs_str(AML_VFM_MAP, "add default decoder ppmgr deinterlace amvideo");
    return 0;
}

static int remove_di()
{
    amsysfs_set_sysfs_str(AML_VFM_MAP, "rm default");
    amsysfs_set_sysfs_str(AML_VFM_MAP, "add default decoder ppmgr amvideo");
    return 0;
}

static int check_add_ppmgr()
{
	char vfm_map[4096] = {0};
    char *s = NULL;
	get_vfm_map_info(vfm_map);
	s = strstr(vfm_map,"default { decoder(0) deinterlace(0) amvideo}");
	if (s != NULL) {
		amsysfs_set_sysfs_str(AML_VFM_MAP, "rm default");
		amsysfs_set_sysfs_str(AML_VFM_MAP, "add default decoder ppmgr deinterlace amvideo");
		LOGI("add ppmgr");
	}

    return 0;
}

static int check_remove_ppmgr()
{
	char vfm_map[4096] = {0};
    char *s = NULL;
	get_vfm_map_info(vfm_map);
	s = strstr(vfm_map,"default { decoder(0) ppmgr(0) deinterlace(0) amvideo}");
	if (s != NULL) {
		amsysfs_set_sysfs_str(AML_VFM_MAP, "rm default");
		amsysfs_set_sysfs_str(AML_VFM_MAP, "add default decoder deinterlace amvideo");
		LOGI("remove ppmgr");
	}

    return 0;
}

#ifdef USE_OPTEEOS
CTsPlayer::CTsPlayer(bool DRMMode, bool omx_player)
#else
CTsPlayer::CTsPlayer(bool omx_player)
#endif
{
    mIsOmxPlayer = omx_player;
    LOGW("CTsPlayer, mIsOmxPlayer: %d\n", mIsOmxPlayer);
}

CTsPlayer::~CTsPlayer()
{
    LOGI("~CTsPlayer()\n");
    if (mIsOmxPlayer) {
        LOGI("Is omx player, return!!!");
        return;
    }
    Stop();
    amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/fatal_error_reset", 0);
    amsysfs_set_sysfs_int("/sys/module/amvideo/parameters/ctsplayer_exist", 0);
    if(perform_flag){
        amsysfs_set_sysfs_str(CPU_SCALING_MODE_NODE,DEFAULT_MODE);
        perform_flag =0;
    }
    m_StopThread = true;
    pthread_join(mThread, NULL);
    pthread_join(mInfoThread, NULL);
    pthread_join(readThread, NULL);
    if(prop_softdemux == 1){
        if(acodec){
            free(acodec);
            acodec = NULL;
        }
        if(vcodec){
            free(vcodec);
            vcodec = NULL;
        }
    }
    QuitIptv(m_isSoftFit, m_isBlackoutPolicy);
}

//取得播放模式,保留，暂不用
int CTsPlayer::GetPlayMode()
{
    LOGI("GetPlayMode\n");
    return 1;
}

int CTsPlayer::SetVideoWindow(int x,int y,int width,int height)
{
    int epg_centre_x = 0;
    int epg_centre_y = 0;
    int old_videowindow_certre_x = 0;
    int old_videowindow_certre_y = 0;
    int new_videowindow_certre_x = 0;
    int new_videowindow_certre_y = 0;
    int new_videowindow_width = 0;
    int new_videowindow_height = 0;
    char vaxis_newx_str[PROPERTY_VALUE_MAX] = {0};
    char vaxis_newy_str[PROPERTY_VALUE_MAX] = {0};
    char vaxis_width_str[PROPERTY_VALUE_MAX] = {0};
    char vaxis_height_str[PROPERTY_VALUE_MAX] = {0};
    int vaxis_newx= -1, vaxis_newy = -1, vaxis_width= -1, vaxis_height= -1;
    int fd_axis, fd_mode;
    int x2 = 0, y2 = 0, width2 = 0, height2 = 0;
    int ret = 0;
    //const char *path_mode = "/sys/class/video/screen_mode";
    const char *path_axis = "/sys/class/video/axis";
    char bcmd[32];
    char buffer[15];
    int mode_w = 0, mode_h = 0;

    LOGI("CTsPlayer::SetVideoWindow: %d, %d, %d, %d\n", x, y, width, height);
	amsysfs_set_sysfs_int("/sys/module/amvideo/parameters/ctsplayer_exist", 1);
	s_video_axis[0] = x;
	s_video_axis[1] = y;
	s_video_axis[2] = width;
	s_video_axis[3] = height;
    OUTPUT_MODE output_mode = get_display_mode();
    if(m_isSoftFit) {
        int x_b=0, y_b=0, w_b=0, h_b=0;
        int mode_x = 0, mode_y = 0, mode_width = 0, mode_height = 0;
        getPosition(output_mode, &mode_x, &mode_y, &mode_width, &mode_height);
        LOGI("SetVideoWindow mode_x: %d, mode_y: %d, mode_width: %d, mode_height: %d\n",
                mode_x, mode_y, mode_width, mode_height);
        /*if(((mode_x == 0) && (mode_y == 0) &&(width < (mode_width -1)) && (height < (mode_height - 1)))
                || (mode_x != 0) || (mode_y != 0)) {
            LOGW("SetVideoWindow this is not full window!\n");
            amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 1);
        } else {
            amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 0);
        }*/
        x_b = x + mode_x;
        y_b = y + mode_y;
        w_b = width + x_b - 1;
        h_b = height + y_b - 1;
        if (h_b < 576 && h_b % 2)
            h_b +=1;
        /*if(m_nEPGWidth !=0 && m_nEPGHeight !=0) {
            amsysfs_set_sysfs_str(path_mode, "1");
        }*/

		if(w_b >= (mode_width + mode_x -1)) w_b = mode_width + mode_x -1 ;
		if(h_b >= (mode_height + mode_y -1)) h_b = mode_height + mode_y -1 ;

        sprintf(bcmd, "%d %d %d %d", x_b, y_b, w_b, h_b);
        subtitleSetSurfaceViewParam(x, y, width, height);
        ret = amsysfs_set_sysfs_str(path_axis, bcmd);
        LOGI("setvideoaxis: %s\n", bcmd);
        return ret;
    }

    /*adjust axis as rate recurrence*/
    GetVideoPixels(mode_w, mode_h);

    x2 = x*mode_w/m_nEPGWidth;
    width2 = width*mode_w/m_nEPGWidth;
    y2 = y*mode_h/m_nEPGHeight;
    height2 = height*mode_h/m_nEPGHeight;

    old_videowindow_certre_x = x2+int(width2/2);
    old_videowindow_certre_y = y2+int(height2/2);

    getPosition(output_mode, &vaxis_newx, &vaxis_newy, &vaxis_width, &vaxis_height);
    LOGI("output_mode: %d, vaxis_newx: %d, vaxis_newy: %d, vaxis_width: %d, vaxis_height: %d\n",
            output_mode, vaxis_newx, vaxis_newy, vaxis_width, vaxis_height);
    epg_centre_x = vaxis_newx+int(vaxis_width/2);
    epg_centre_y = vaxis_newy+int(vaxis_height/2);
    new_videowindow_certre_x = epg_centre_x + int((old_videowindow_certre_x-mode_w/2)*vaxis_width/mode_w);
    new_videowindow_certre_y = epg_centre_y + int((old_videowindow_certre_y-mode_h/2)*vaxis_height/mode_h);
    new_videowindow_width = int(width2*vaxis_width/mode_w);
    new_videowindow_height = int(height2*vaxis_height/mode_h);
    LOGI("CTsPlayer::mode_w = %d, mode_h = %d, mw = %d, mh = %d \n",
            mode_w, mode_h, m_nEPGWidth, m_nEPGHeight);

    /*if(m_nEPGWidth !=0 && m_nEPGHeight !=0) {
        amsysfs_set_sysfs_str(path_mode, "1");
    }*/

    sprintf(bcmd, "%d %d %d %d", new_videowindow_certre_x-int(new_videowindow_width/2)-1,
            new_videowindow_certre_y-int(new_videowindow_height/2)-1,
            new_videowindow_certre_x+int(new_videowindow_width/2)+1,
            new_videowindow_certre_y+int(new_videowindow_height/2)+1);

    ret = amsysfs_set_sysfs_str(path_axis, bcmd);
    LOGI("setvideoaxis: %s\n", bcmd);

    if((width2 > 0)&&(height2 > 0)&&((width2 < (mode_w -10))||(height2< (mode_h -10))))
        amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_hd",1);
    else
        amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_hd",0);
    return ret;
}

int CTsPlayer::VideoShow(void)
{
    LOGI("VideoShow\n");
    //amsysfs_set_sysfs_str("/sys/class/graphics/fb0/video_hole", "0 0 1280 720 0 8");
    if(amsysfs_get_sysfs_int("/sys/class/video/disable_video") == 1)
        amsysfs_set_sysfs_int("/sys/class/video/disable_video",2);
    else
        LOGW("video is enable, no need to set disable_video again\n");
    return 0;
}

int CTsPlayer::VideoHide(void)
{
    LOGI("VideoHide\n");
    //amsysfs_set_sysfs_str("/sys/class/graphics/fb0/video_hole", "0 0 0 0 0 0");
    amsysfs_set_sysfs_int("/sys/class/video/disable_video",2);
    return 0;
}

void CTsPlayer::InitVideo(PVIDEO_PARA_T pVideoPara)
{
    vPara=*pVideoPara;
#ifdef TELECOM_VFORMAT_SUPPORT
    vPara.vFmt = changeVformat(vPara.vFmt);
#endif
    LOGI("InitVideo vPara->pid: %d, vPara->vFmt: %d\n", vPara.pid, vPara.vFmt);
}

void CTsPlayer::InitAudio(PAUDIO_PARA_T pAudioPara)
{
    PAUDIO_PARA_T pAP = pAudioPara;
    int count = 0;

    LOGI("InitAudio, pAP=%d, pAP->pid=%d, pAP->samplate=%d\n", pAP, pAP->pid, pAP->nSampleRate);
    memset(a_aPara,0,sizeof(AUDIO_PARA_T)*MAX_AUDIO_PARAM_SIZE);
    while((pAP->pid != 0 || pAP->nSampleRate != 0)&&(count<MAX_AUDIO_PARAM_SIZE)) {
#ifdef TELECOM_VFORMAT_SUPPORT
        pAP->aFmt = changeAformat(pAP->aFmt);
#endif
        a_aPara[count]= *pAP;
        LOGI("InitAudio pAP->pid: %d, pAP->aFmt: %d, channel=%d, samplerate=%d\n", pAP->pid, pAP->aFmt, pAP->nChannels, pAP->nSampleRate);
        pAP++;
        count++;
    }
    return ;
}

void CTsPlayer::InitSubtitle(PSUBTITLE_PARA_T pSubtitlePara)
{
    int count = 0;

    LOGI("InitSubtitle\n");
    memset(sPara,0,sizeof(SUBTITLE_PARA_T)*MAX_SUBTITLE_PARAM_SIZE);
    while((pSubtitlePara->pid != 0)&&(count<MAX_SUBTITLE_PARAM_SIZE)) {
        sPara[count]= *pSubtitlePara;
        LOGI("InitSubtitle pSubtitlePara->pid:%d\n",pSubtitlePara->pid);
        pSubtitlePara++;
        count++;
    }
    amsysfs_set_sysfs_int("/sys/class/subtitle/total",count);
    return ;
}

void setSubType(PSUBTITLE_PARA_T pSubtitlePara)
{
    if(!pSubtitlePara)
        return;
    LOGI("setSubType pSubtitlePara->pid:%d pSubtitlePara->sub_type:%d\n",pSubtitlePara->pid,pSubtitlePara->sub_type);
    if (pSubtitlePara->sub_type== CTC_CODEC_ID_DVD_SUBTITLE) {
        set_subtitle_subtype(0);
    } else if (pSubtitlePara->sub_type== CTC_CODEC_ID_HDMV_PGS_SUBTITLE) {
        set_subtitle_subtype(1);
    } else if (pSubtitlePara->sub_type== CTC_CODEC_ID_XSUB) {
        set_subtitle_subtype(2);
    } else if (pSubtitlePara->sub_type == CTC_CODEC_ID_TEXT || \
                pSubtitlePara->sub_type == CTC_CODEC_ID_SSA) {
        set_subtitle_subtype(3);
    } else if (pSubtitlePara->sub_type == CTC_CODEC_ID_DVB_SUBTITLE) {
        set_subtitle_subtype(5);
    } else {
        set_subtitle_subtype(4);
    }
}

#define FILTER_AFMT_MPEG		(1 << 0)
#define FILTER_AFMT_PCMS16L	    (1 << 1)
#define FILTER_AFMT_AAC			(1 << 2)
#define FILTER_AFMT_AC3			(1 << 3)
#define FILTER_AFMT_ALAW		(1 << 4)
#define FILTER_AFMT_MULAW		(1 << 5)
#define FILTER_AFMT_DTS			(1 << 6)
#define FILTER_AFMT_PCMS16B		(1 << 7)
#define FILTER_AFMT_FLAC		(1 << 8)
#define FILTER_AFMT_COOK		(1 << 9)
#define FILTER_AFMT_PCMU8		(1 << 10)
#define FILTER_AFMT_ADPCM		(1 << 11)
#define FILTER_AFMT_AMR			(1 << 12)
#define FILTER_AFMT_RAAC		(1 << 13)
#define FILTER_AFMT_WMA			(1 << 14)
#define FILTER_AFMT_WMAPRO		(1 << 15)
#define FILTER_AFMT_PCMBLU		(1 << 16)
#define FILTER_AFMT_ALAC		(1 << 17)
#define FILTER_AFMT_VORBIS		(1 << 18)
#define FILTER_AFMT_AAC_LATM		(1 << 19)
#define FILTER_AFMT_APE		       (1 << 20)
#define FILTER_AFMT_EAC3		       (1 << 21)

int TsplayerGetAFilterFormat(const char *prop)
{
    char value[PROPERTY_VALUE_MAX];
    int filter_fmt = 0;
    /* check the dts/ac3 firmware status */
    if(access("/system/etc/firmware/audiodsp_codec_ddp_dcv.bin",F_OK) && access("/system/lib/libstagefright_soft_dcvdec.so",F_OK)){
        filter_fmt |= (FILTER_AFMT_AC3|FILTER_AFMT_EAC3);
    }
    if(access("/system/etc/firmware/audiodsp_codec_dtshd.bin",F_OK) && access("/system/lib/libstagefright_soft_dtshd.so",F_OK)){
        filter_fmt  |= FILTER_AFMT_DTS;
    }
    if(property_get(prop, value, NULL) > 0) {
        LOGI("[%s:%d]disable_adec=%s\n", __FUNCTION__, __LINE__, value);
        if(strstr(value,"mpeg") != NULL || strstr(value,"MPEG") != NULL) {
            filter_fmt |= FILTER_AFMT_MPEG;
        }
        if(strstr(value,"pcms16l") != NULL || strstr(value,"PCMS16L") != NULL) {
            filter_fmt |= FILTER_AFMT_PCMS16L;
        }
        if(strstr(value,"aac") != NULL || strstr(value,"AAC") != NULL) {
            filter_fmt |= FILTER_AFMT_AAC;
        }
        if(strstr(value,"ac3") != NULL || strstr(value,"AC#") != NULL) {
            filter_fmt |= FILTER_AFMT_AC3;
        }
        if(strstr(value,"alaw") != NULL || strstr(value,"ALAW") != NULL) {
            filter_fmt |= FILTER_AFMT_ALAW;
        }
        if(strstr(value,"mulaw") != NULL || strstr(value,"MULAW") != NULL) {
            filter_fmt |= FILTER_AFMT_MULAW;
        }
        if(strstr(value,"dts") != NULL || strstr(value,"DTS") != NULL) {
            filter_fmt |= FILTER_AFMT_DTS;
        }
        if(strstr(value,"pcms16b") != NULL || strstr(value,"PCMS16B") != NULL) {
            filter_fmt |= FILTER_AFMT_PCMS16B;
        }
        if(strstr(value,"flac") != NULL || strstr(value,"FLAC") != NULL) {
            filter_fmt |= FILTER_AFMT_FLAC;
        }
        if(strstr(value,"cook") != NULL || strstr(value,"COOK") != NULL) {
            filter_fmt |= FILTER_AFMT_COOK;
        }
        if(strstr(value,"pcmu8") != NULL || strstr(value,"PCMU8") != NULL) {
            filter_fmt |= FILTER_AFMT_PCMU8;
        }
        if(strstr(value,"adpcm") != NULL || strstr(value,"ADPCM") != NULL) {
            filter_fmt |= FILTER_AFMT_ADPCM;
        }
        if(strstr(value,"amr") != NULL || strstr(value,"AMR") != NULL) {
            filter_fmt |= FILTER_AFMT_AMR;
        }
        if(strstr(value,"raac") != NULL || strstr(value,"RAAC") != NULL) {
            filter_fmt |= FILTER_AFMT_RAAC;
        }
        if(strstr(value,"wma") != NULL || strstr(value,"WMA") != NULL) {
            filter_fmt |= FILTER_AFMT_WMA;
        }
        if(strstr(value,"wmapro") != NULL || strstr(value,"WMAPRO") != NULL) {
            filter_fmt |= FILTER_AFMT_WMAPRO;
        }
        if(strstr(value,"pcmblueray") != NULL || strstr(value,"PCMBLUERAY") != NULL) {
            filter_fmt |= FILTER_AFMT_PCMBLU;
        }
        if(strstr(value,"alac") != NULL || strstr(value,"ALAC") != NULL) {
            filter_fmt |= FILTER_AFMT_ALAC;
        }
        if(strstr(value,"vorbis") != NULL || strstr(value,"VORBIS") != NULL) {
            filter_fmt |= FILTER_AFMT_VORBIS;
        }
        if(strstr(value,"aac_latm") != NULL || strstr(value,"AAC_LATM") != NULL) {
            filter_fmt |= FILTER_AFMT_AAC_LATM;
        }
        if(strstr(value,"ape") != NULL || strstr(value,"APE") != NULL) {
            filter_fmt |= FILTER_AFMT_APE;
        }
        if(strstr(value,"eac3") != NULL || strstr(value,"EAC3") != NULL) {
            filter_fmt |= FILTER_AFMT_EAC3;
        }
    }
    LOGI("[%s:%d]filter_afmt=%x\n", __FUNCTION__, __LINE__, filter_fmt);
    return filter_fmt;
}

pthread_t mSetSubRatioThread;
int mSubRatioRetry = 200; //10*0.5s=5s to retry get video width and height
bool mSubRatioThreadStop = false;
void *setSubRatioAutoThread(void *pthis)
{
    int width = -1;
    int height = -1;
    int videoWidth = -1;
    int videoHeight = -1;
    int dispWidth = -1;
    int dispHeight = -1;
    int frameWidth = -1;
    int frameHeight = -1;

    int mode_x = 0;
    int mode_y = 0;
    int mode_width = 0;
    int mode_height = 0;
    vdec_status vdec;

    char pts_video_chr[64] = {0};
    char pts_video_chr_bac[64] = {0};
    bool pts_video_stored = false;

    do {
        amsysfs_get_sysfs_str("/sys/class/tsync/pts_video", pts_video_chr, 64);
        LOGE("CTsPlayer::setSubRatioAutoThread mSubRatioRetry:%d, pts_video_chr:%s\n",mSubRatioRetry, pts_video_chr);
        if (strlen(pts_video_chr) > 0)
        {
            LOGE("CTsPlayer::setSubRatioAutoThread mSubRatioRetry:%d, pts_video_chr:%s, pts_video_stored:%d\n",mSubRatioRetry, pts_video_chr, pts_video_stored);
            if (strcmp(pts_video_chr, "0x0") && !pts_video_stored)
            {
                pts_video_stored = true;
                strcpy(pts_video_chr_bac, pts_video_chr);
                continue;
            }
            else if (strlen(pts_video_chr_bac) > 0)
            {
                LOGE("CTsPlayer::setSubRatioAutoThread mSubRatioRetry:%d, pts_video_chr:%s, pts_video_chr_bac:%s,pts_video_stored:%d\n",mSubRatioRetry, pts_video_chr, pts_video_chr_bac, pts_video_stored);
                if (strcmp(pts_video_chr_bac, pts_video_chr))
                {
                    break;
                }
            }
        }
        //videoWidth = amsysfs_get_sysfs_int("/sys/class/video/frame_width");
        //videoHeight = amsysfs_get_sysfs_int("/sys/class/video/frame_height");
        //LOGE("CTsPlayer::setSubRatioAutoThread (videoWidth,videoHeight):(%d,%d), mSubRatioRetry:%d, pts_video:%d, pts_video_chr:%s\n",videoWidth, videoHeight, mSubRatioRetry, pts_video, pts_video_chr);

        mSubRatioRetry--;
        usleep(50000); // 0.05 s
    }while(mSubRatioRetry > 0 && !mSubRatioThreadStop);

    if (mSubRatioThreadStop)
    {
        return NULL;
    }

    videoWidth = amsysfs_get_sysfs_int("/sys/class/video/frame_width");
    videoHeight = amsysfs_get_sysfs_int("/sys/class/video/frame_height");
    LOGE("CTsPlayer::setSubRatioAutoThread 00(videoWidth,videoHeight):(%d,%d), mSubRatioRetry:%d, pts_video_chr:%s\n",videoWidth, videoHeight, mSubRatioRetry, pts_video_chr);

    DisplayInfo info;
    sp<IBinder> display(SurfaceComposerClient::getBuiltInDisplay(
                ISurfaceComposer::eDisplayIdMain));
    SurfaceComposerClient::getDisplayInfo(display, &info);
    frameWidth = info.w;
    frameHeight = info.h;
   LOGI("CTsPlayer::StartPlay (frameWidth,frameHeight):(%d,%d)\n",info.w, info.h);


    OUTPUT_MODE output_mode = get_display_mode();
    getPosition(output_mode, &mode_x, &mode_y, &mode_width, &mode_height);
    dispWidth = mode_width - mode_x;
    dispHeight = mode_height - mode_y;
    LOGI("CTsPlayer::StartPlay (dispWidth,dispHeight):(%d,%d)\n",dispWidth, dispHeight);

    // full screen
    width = dispWidth;
    height = dispHeight;

    width = width * frameWidth / dispWidth;
    height = height * frameHeight / dispHeight;
    float ratioW = 1.000f;
    float ratioH = 1.000f;
    float ratioMax = 2.000f;
    float ratioMin = 1.250f;
    int maxW = dispWidth;
    int maxH = dispHeight;
    if (videoWidth != 0 & videoHeight != 0) {
        ratioW = ((float)width) / videoWidth;
        ratioH = ((float)height) / videoHeight;
        if (ratioW > ratioMax || ratioH > ratioMax) {
            ratioW = ratioMax;
            ratioH = ratioMax;
        }
        /*else if (ratioW < ratioMin || ratioH < ratioMin) {
            ratioW = ratioMin;
            ratioH = ratioMin;
        }*/
        LOGE("CTsPlayer::StartPlay (ratioW,ratioH):(%f,%f),(maxW,maxH):(%d,%d)\n", ratioW, ratioH, maxW, maxH);
        subtitleSetImgRatio(ratioW, ratioH, maxW, maxH);
    }
    return NULL;
}

void setSubRatioAuto()
{
    mSubRatioThreadStop = false;
    pthread_create(&mSetSubRatioThread, NULL, setSubRatioAutoThread, NULL);
 }

/*
 * player_startsync_set
 *
 * reset start sync using prop media.amplayer.startsync.mode
 * 0 none start sync
 * 1 slow sync repeate mode
 * 2 drop pcm mode
 *
 * */

int player_startsync_set(int mode)
{
    const char * startsync_mode = "/sys/class/tsync/startsync_mode";
    const char * droppcm_prop = "sys.amplayer.drop_pcm"; // default enable
    const char * slowsync_path = "/sys/class/tsync/slowsync_enable";
    const char * slowsync_repeate_path = "/sys/class/video/slowsync_repeat_enable";

/*
    char value[PROPERTY_VALUE_MAX];
    int mode = get_sysfs_int(startsync_mode);
    int ret = property_get(startsync_prop, value, NULL);
    if (ret <= 0) {
        log_print("start sync mode prop not setting ,using default none \n");
    }
    else
        mode = atoi(value);
*/
    LOGI("start sync mode desp: 0 -none 1-slowsync repeate 2-droppcm \n");
    LOGI("start sync mode = %d \n",mode);

    if(mode == 0) // none case
    {
        set_sysfs_int(slowsync_path,0);
        //property_set(droppcm_prop, "0");
        set_sysfs_int(slowsync_repeate_path,0);
    }

    if(mode == 1) // slow sync repeat mode
    {
        set_sysfs_int(slowsync_path,1);
        //property_set(droppcm_prop, "0");
        set_sysfs_int(slowsync_repeate_path,1);
    }

    if(mode == 2) // drop pcm mode
    {
        set_sysfs_int(slowsync_path,0);
        //property_set(droppcm_prop, "1");
        set_sysfs_int(slowsync_repeate_path,0);
    }

    return 0;
}
bool CTsPlayer::StartPlay(){
        int ret;
        if(prop_start_no_out){// start with no out  mode
            set_sysfs_int("/sys/class/video/show_first_frame_nosync", 0);	//keep last frame instead of show first frame
            pcodec->start_no_out = 1;
        }else{
            pcodec->start_no_out = 0;
        }

        mLastVdecInfoNum = -1;
        memset(&m_sCtsplayerState, 0, sizeof(struct ctsplayer_state));
        m_sCtsplayerState.video_ratio = -1;

        ret = iStartPlay();
        codec_set_freerun_mode(pcodec, 0);
        if (prop_trickmode_debug) {
            LOGI("debug enter fast mode\n");
            Fast();
            LOGI("debug leave fast mode\n");
        }
        return ret;
}

bool CTsPlayer::iStartPlay()
{
    int ret;
    int filter_afmt;
    char vaule[PROPERTY_VALUE_MAX] = {0};
    char vfm_map[4096] = {0};
    char *s = NULL;
    char *p = NULL;
    int sleep_number = 0;
    int video_buf_used = 0;
    int audio_buf_used = 0;
    int subtitle_buf_used = 0;
    int userdata_buf_used = 0;
    int start_no_out = 0;
#ifdef USE_OPTEEOS
    int tvpdrm = 1;
#endif
    lp_lock(&mutex);
    if (m_bIsPlay) {
        LOGE("[%s:%d]Already StartPlay: m_bIsPlay=%s\n", __FUNCTION__, __LINE__, (m_bIsPlay ? "true" : "false"));
        lp_unlock(&mutex);
        return true;
    }
    amsysfs_set_sysfs_str(CPU_SCALING_MODE_NODE,SCALING_MODE);
    perform_flag =1;
    amsysfs_set_sysfs_int("/sys/class/tsync/enable", 1);
    set_sysfs_int("/sys/class/tsync/vpause_flag",0); // reset vpause flag -> 0

    // start with no out  mode
    start_no_out = pcodec->start_no_out;
    if (start_no_out) {
        set_sysfs_int("/sys/class/video/show_first_frame_nosync", 0);
    }else{
          set_sysfs_int("/sys/class/video/show_first_frame_nosync", prop_show_first_frame_nosync);	//keep last frame instead of show first frame
     }


    pcodec = &codec;
    memset(pcodec,0,sizeof(*pcodec));
    pcodec->stream_type = STREAM_TYPE_TS;
    pcodec->video_type = vPara.vFmt;
    pcodec->has_video = 1;
    pcodec->audio_type = a_aPara[0].aFmt;
    pcodec->start_no_out = start_no_out;

    property_get("iptv.hasaudio", vaule, "1");
    hasaudio = atoi(vaule);

    memset(vaule, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.hasvideo", vaule, "1");
    hasvideo = atoi(vaule);


    /*if(pcodec->audio_type == AFORMAT_AAC_LATM) {
        pcodec->audio_type = AFORMAT_EAC3;
    }*/

    if(IS_AUIDO_NEED_EXT_INFO(pcodec->audio_type)) {
        pcodec->audio_info.valid = 1;
        LOGI("set audio_info.valid to 1");
    }

    amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/error_skip_reserve",H264_error_skip_reserve);
	amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/error_skip_divisor",0);
    if (!m_bFast) {
        amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/error_recovery_mode", 0);
        if((int)a_aPara[0].pid != 0) {
            pcodec->has_audio = 1;
            pcodec->audio_pid = (int)a_aPara[0].pid;
			//pcodec->audio_samplerate=a_aPara[0].nSampleRate;
	        //pcodec->audio_channels=a_aPara[0].nChannels;
        }

        LOGI("pcodec->audio_samplerate: %d, pcodec->audio_channels: %d\n",
            pcodec->audio_samplerate, pcodec->audio_channels);

        if((prop_softdemux == 0 && (int)sPara[0].pid != 0) || (prop_softdemux == 1 && (int)sPara[0].sub_type != 0)) {
            pcodec->has_sub = 1;
            pcodec->sub_pid = (int)sPara[0].pid;
            setSubType(&sPara[0]);
        }
        LOGI("pcodec->sub_pid: %d \n", pcodec->sub_pid);
    } else {
        amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/error_recovery_mode", 2);
        amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/error_skip_reserve",0);
        pcodec->has_audio = 0;
        pcodec->audio_pid = -1;
    }

    pcodec->video_pid = (int)vPara.pid;
    if(pcodec->video_type == VFORMAT_H264 && !s_h264sameucode) {
        pcodec->am_sysinfo.format = VIDEO_DEC_FORMAT_H264;
        lpbuffer_st.buffer = (unsigned char *)malloc(CTC_BUFFER_LOOP_NSIZE*buffersize);
        if(lpbuffer_st.buffer == NULL) {
            LOGI("malloc failed\n");
            lpbuffer_st.enlpflag = false;
            lpbuffer_st.rp = NULL;
            lpbuffer_st.wp = NULL;
        } else{
            LOGI("malloc success\n");
            lp_lock_init(&mutex_lp, NULL);
            lp_lock(&mutex_lp);
            lpbuffer_st.enlpflag = true;
            lpbuffer_st.rp = lpbuffer_st.buffer;
            lpbuffer_st.wp = lpbuffer_st.buffer;
            lpbuffer_st.bufferend = lpbuffer_st.buffer + CTC_BUFFER_LOOP_NSIZE*buffersize;
            lpbuffer_st.valid_can_read = 0;
            memset(lpbuffer_st.buffer, 0, CTC_BUFFER_LOOP_NSIZE*buffersize);
            lp_unlock(&mutex_lp);
        }

		/*if(m_bFast){
			pcodec->am_sysinfo.param=(void *)am_sysinfo_param;
			pcodec->am_sysinfo.height = vPara.nVideoHeight;
			pcodec->am_sysinfo.width = vPara.nVideoWidth;
		}
		else{
        	pcodec->am_sysinfo.param = (void *)(0);
		}*/
		pcodec->am_sysinfo.param = (void *)(0);
    }else if(pcodec->video_type == VFORMAT_H264_4K2K) {
        pcodec->am_sysinfo.format = VIDEO_DEC_FORMAT_H264_4K2K;
    }

    if(pcodec->video_type == VFORMAT_MPEG4) {
        pcodec->am_sysinfo.format= VIDEO_DEC_FORMAT_MPEG4_5;
        LOGI("VIDEO_DEC_FORMAT_MPEG4_5\n");
    }

    filter_afmt = TsplayerGetAFilterFormat("media.amplayer.disable-acodecs");
    if(((1 << pcodec->audio_type) & filter_afmt) != 0) {
        LOGI("## filtered format audio_format=%d,----\n", pcodec->audio_type);
        pcodec->has_audio = 0;
    }
    if(hasaudio == 0)
        pcodec->has_audio = 0;
    if(hasvideo == 0)
        pcodec->has_video = 0;
    LOGI("set vFmt:%d, aFmt:%d, vpid:%d, apid:%d\n", vPara.vFmt, a_aPara[0].aFmt, vPara.pid, a_aPara[0].pid);
    LOGI("set has_video:%d, has_audio:%d, video_pid:%d, audio_pid:%d\n", pcodec->has_video, pcodec->has_audio,
            pcodec->video_pid, pcodec->audio_pid);
    pcodec->noblock = 0;

    char value[PROPERTY_VALUE_MAX] = {0};
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("iptv.dumpfile", value, "0");
    prop_dumpfile = atoi(value);

    if(prop_dumpfile){
        if(m_fp == NULL) {
            char tmpfilename[1024] = "";
            static int tmpfileindex = 0;
            memset(vaule, 0, PROPERTY_VALUE_MAX);
            property_get("iptv.dumppath", vaule, "/storage/external_storage/sda1");
            sprintf(tmpfilename, "%s/Live%d.ts", vaule, tmpfileindex);
            tmpfileindex++;
            m_fp = fopen(tmpfilename, "wb+");
        }
    }

    // enable avsync only when av both exists, not including trick
    if(hasaudio && hasvideo)
        player_startsync_set(2);

    if(prop_softdemux == 1){
        if(pcodec->has_video){
            vcodec->has_video  = 1;
            vcodec->video_type = pcodec->video_type;
            vcodec->video_pid  = pcodec->video_pid;
            vcodec->stream_type = STREAM_TYPE_ES_VIDEO;
            if (CheckMultiSupported(pcodec->video_type) == true) {
                vcodec->dec_mode = STREAM_TYPE_STREAM;
            } else {
                vcodec->dec_mode = STREAM_TYPE_SINGLE;
            }

            LOGI("Init the vcodec parameters:video_type:%d,video_pid:%d\n",
            vcodec->video_type, vcodec->video_pid);
        }
        if(pcodec->has_audio){
            acodec->has_audio = 1;
            acodec->audio_type = pcodec->audio_type;
            acodec->audio_pid  = pcodec->audio_pid;
            acodec->stream_type = STREAM_TYPE_ES_AUDIO;
            acodec->audio_channels = pcodec->audio_channels;
            acodec->audio_samplerate = pcodec->audio_samplerate;
            LOGI("Init the acodec parameters:audio_type:%d,audio_pid:%d, channel=%d, samplate=%d\n",
            acodec->audio_type,acodec->audio_pid, acodec->audio_channels, acodec->audio_samplerate);
        }
#if 1
        if (prop_esdata != 1) {
            if (pipe(pipe_fd) == -1) {
                perror("pipe");
                exit(1);
            } else {
                fcntl(pipe_fd[0], F_SETPIPE_SZ, 1048576);
                fcntl(pipe_fd[1], F_SETPIPE_SZ, 1048576);
                LOGD("pipe opened!");

                LOGI("set pipe read block\n");
            }
        }
#endif
    }

    if (prop_multi_play == 0) {
        do{
    		get_vfm_map_info(vfm_map);
    		s = strstr(vfm_map,"(1)");
    		p = strstr(vfm_map,"ionvideo}");
    		video_buf_used=amsysfs_get_sysfs_int("/sys/class/amstream/videobufused");
    		audio_buf_used=amsysfs_get_sysfs_int("/sys/class/amstream/audiobufused");
    		subtitle_buf_used=amsysfs_get_sysfs_int("/sys/class/amstream/subtitlebufused");
    		userdata_buf_used=amsysfs_get_sysfs_int("/sys/class/amstream/userdatabufused");
    		LOGI("s=%s,p=%s\n",s,p);
    		LOGI("buf used video:%d,audio:%d,subtitle:%d,userdata:%d\n",
    			video_buf_used,audio_buf_used,subtitle_buf_used,userdata_buf_used);
    		if((s == NULL)&&(p == NULL)&&(video_buf_used==0)&&(audio_buf_used==0)&&
    	   		(subtitle_buf_used==0)&&(userdata_buf_used==0))
        		LOGI("not find valid,begin init\n");
    		else{
    			sleep_number++;
    			usleep(50*1000);
        		LOGI("find find find,sleep_number=%d\n",sleep_number);
    		}
        }while((s != NULL)||(p != NULL)||(video_buf_used != 0)||(audio_buf_used != 0) ||
                (subtitle_buf_used != 0)||(userdata_buf_used != 0));
    }

    //check_remove_ppmgr();
    if(prop_softdemux == 0) {

		if (CheckMultiSupported(pcodec->video_type) == true) {
			pcodec->dec_mode = STREAM_TYPE_STREAM;
		} else {
			pcodec->dec_mode = STREAM_TYPE_SINGLE;
		}
        ret = codec_init(pcodec);
    } else{
#ifdef USE_OPTEEOS
        memset(vaule, 0, PROPERTY_VALUE_MAX);
        property_get("iptv.tvpdrm", vaule, "1");
        tvpdrm = atoi(vaule);
        LOGE("prop_tvpdrm :%d, 1 tvp and 0 is no tvp debug \n",tvpdrm);
        if (tvpdrm == 1) {
            amsysfs_set_sysfs_int("/sys/class/video/blackout_policy",1);
            PA_free_cma_buffer();
            PA_Tvpsecmen();
            amsysfs_set_sysfs_str( "/sys/class/vfm/map", "rm default");
            amsysfs_set_sysfs_str( "/sys/class/vfm/map", "add default decoder deinterlace  amvideo");
        }
#endif
        if (pcodec->has_video) {
            ret = codec_init(vcodec);
        }
        if (pcodec->has_audio) {
            ret = codec_init(acodec);
        }
        LOGI("Init audio,hasaudio:%d\n",pcodec->has_audio);
        if ((pcodec->has_audio) && (acodec != NULL)){
            pcodec = acodec;
            if(vcodec != NULL){
                pcodec->has_video = 1;
                pcodec->video_type = vcodec->video_type;
                pcodec->video_pid  = vcodec->video_pid;
                pcodec->stream_type = STREAM_TYPE_ES_VIDEO;
            }
            LOGI("[%s:%d]init pcodec pointer to acodec!\n", __FUNCTION__, __LINE__);
        }else if ((pcodec->has_video) && (vcodec != NULL)){
            pcodec = vcodec;
        }
    }
    LOGI("StartPlay codec_init After: %d\n", ret);
    if(ret == 0) {
        if (m_isBlackoutPolicy)
            amsysfs_set_sysfs_int("/sys/class/video/blackout_policy", 0);
        else
            amsysfs_set_sysfs_int("/sys/class/video/blackout_policy", 1);
        m_bIsPlay = true;
        m_bIsPause = false;
        keep_vdec_mem = 0;
        amsysfs_set_sysfs_int("/sys/class/vdec/keep_vdec_mem", 1);
        /*if(!m_bFast) {
            LOGI("StartPlay: codec_pause to buffer sometime");
            codec_pause(pcodec);
        }*/
    }
    //init tsync_syncthresh
    codec_set_cntl_syncthresh(pcodec, pcodec->has_audio);

    if(amsysfs_get_sysfs_int("/sys/class/video/slowsync_flag")!=1){
        amsysfs_set_sysfs_int("/sys/class/video/slowsync_flag",1);
    }
    //amsysfs_set_sysfs_str("/sys/class/graphics/fb0/video_hole","0 0 1280 720 0 8");
    m_bWrFirstPkg = true;
    m_bchangeH264to4k = false;
    writecount = 0;
#ifdef USE_OPTEEOS
   if(pcodec->has_video && pcodec->video_type == VFORMAT_HEVC&&tvpdrm==1&&prop_softdemux == 1) {
       amsysfs_set_sysfs_int("/sys/class/video/blackout_policy",1);
    }
#endif
    m_StartPlayTimePoint = av_gettime();
    LOGI("StartPlay: m_StartPlayTimePoint = %lld\n", m_StartPlayTimePoint);
    LOGI("subtitleSetSurfaceViewParam 1\n");
    if (pcodec->has_sub == 1) {
	LOGI("subtitleSetSurfaceViewParam\n");
        subtitleSetSurfaceViewParam(s_video_axis[0], s_video_axis[1], s_video_axis[2], s_video_axis[3]);
        subtitleResetForSeek();
        subtitleOpen("", this);// "" fit for api param, no matter what the path is for inner subtitle.
        subtitleShow();
        //setSubRatioAuto();// 1.this function in subtitleservice is do nothing;2.don't free thread resource
    }
    lp_unlock(&mutex);
    for (int i = 0; i<4; i++) {
        ALOGE("call update_nativewindow");
        update_nativewindow();
    }
    return !ret;
}

bool CTsPlayer::CheckMultiSupported(int video_type){
	int multi_dec_support = 1;
    char value[92];
    if(property_get("media.ctcplayer.enable", value, NULL) > 0) {
        sscanf(value, "%d", &multi_dec_support);
        ALOGD("media.ctcplayer.enable=%d", multi_dec_support);
    } else
        ALOGW("Can not read property media.ctcplayer.enable, using %d\n", multi_dec_support);
    if ((video_type != VFORMAT_HEVC) && (video_type != VFORMAT_H264)) {
	    ALOGI("CheckMultiSupported --video_type:%d\n", video_type);
		multi_dec_support = 0;
	}
    if(multi_dec_support) {
		return true;
	} else {
		return false;
	}
}


#if 0
int CTsPlayer::SoftWriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp)
{
    int ret = -1;
    int temp_size = 0;
    static int retry_count = 0;
    buf_status audio_buf;
    buf_status video_buf;
    float audio_buf_level = 0.00f;
    float video_buf_level = 0.00f;
    if(!m_bIsPlay || (m_bchangeH264to4k && !s_h264sameucode))
        return -1;
    codec_para_t *pCodec = NULL;
    //LOGI("--SoftWriteData, type=%d, nsize=%u, timestamp=0x%llx\n", type, nSize, timestamp);
#if 1
	if (type == PLAYER_STREAMTYPE_AUDIO) {
        pCodec = acodec;
	} else if (type == PLAYER_STREAMTYPE_VIDEO) {
        pCodec = vcodec;
	} else if (type == PLAYER_STREAMTYPE_SUBTITLE) {
		pCodec = scodec;
	} else {
		pCodec = vcodec;
	}

    if (pcodec->has_audio) {
        codec_get_abuf_state(acodec, &audio_buf);
    }

    if (pcodec->has_video) {
        codec_get_vbuf_state(vcodec, &video_buf);
    }

    if(audio_buf.size != 0)
        audio_buf_level = (float)audio_buf.data_len / audio_buf.size;
    if(video_buf.size != 0)
        video_buf_level = (float)video_buf.data_len / video_buf.size;


    if((audio_buf_level >= 0.8) || (video_buf_level >= 0.8)) {
        LOGI("SoftWriteData: alevel= %.5f,len=%d,size=%d, vlevel=%.5f,len=%d,size=%d, has_audio=%d,has_video=%d,Don't writedate()\n",
            audio_buf_level, audio_buf.data_len, audio_buf.size, video_buf_level, video_buf.data_len, video_buf.size, pcodec->has_audio, pcodec->has_video);
        return -1;
    }

#endif
    lp_lock(&mutex);
    for(int retry_count=0; retry_count<50; retry_count++) {
        ret = codec_write(pCodec, pBuffer+temp_size, nSize-temp_size);
        if((ret < 0) || (ret > nSize)) {
            if(ret < 0 && (errno == EAGAIN)) {
                usleep(100);
                LOGI("SoftWriteData: codec_write return EAGAIN!, retry_count=%d\n", retry_count);
                continue;
            } else {
                LOGI("SoftWriteData: codec_write return %d!\n", ret);
                if(pcodec->handle > 0){
                    ret = codec_close(pcodec);
                    ret = codec_init(pcodec);
                    if(m_bFast) {
                        codec_set_mode(pcodec, TRICKMODE_I);
                    }
                    LOGI("SoftWriteData : codec need close and reinit m_bFast=%d\n", m_bFast);
                } else {
                    LOGI("SoftWriteData: codec_write return error or stop by called!\n");
                    break;
                }
            }
        } else {
            temp_size += ret;
            if(prop_write_log == 1)
                LOGI("SoftWriteData: codec_write  nSize is %d! temp_size=%d retry_count=%d\n", nSize, temp_size, retry_count);
            if(temp_size >= nSize) {
                temp_size = nSize;
                break;
            }
            usleep(2000);
        }
        if (ret >= 0 && temp_size > ret)
            ret = temp_size; // avoid write size large than 64k size
    }
    lp_unlock(&mutex);
    if(ret > 0) {
        if((m_fp != NULL) && (temp_size > 0)) {
            fwrite(pBuffer, 1, temp_size, m_fp);
            LOGI("ret[%d] temp_size[%d] nSize[%d]\n", ret, temp_size, nSize);
        }
        if(writecount >= MAX_WRITE_COUNT) {
            m_bWrFirstPkg = false;
            writecount = 0;
        }
        if(m_bWrFirstPkg == true) {
            writecount++;
        }
    } else {
        LOGW("SoftWriteData: codec_write fail(%d),temp_size[%d] nSize[%d]\n", ret, temp_size, nSize);
		if(temp_size > 0) {
			if(m_fp != NULL)
				fwrite(pBuffer, 1, temp_size, m_fp);
			return temp_size;
		}
        return -1;
    }
    return ret;
}
#endif


int CTsPlayer::SoftWriteData(PLAYER_STREAMTYPE_E type, uint8_t *pBuffer, uint32_t nSize, uint64_t timestamp) {
    int ret = -1;
    static int retry_count = 0;
    buf_status audio_buf;
    buf_status video_buf;
    float audio_buf_level = 0.00f;
    float video_buf_level = 0.00f;
    codec_para_t *pcodec;
	char value[PROPERTY_VALUE_MAX] = {0};

    if(!m_bIsPlay)
        return -1;
	if (type == PLAYER_STREAMTYPE_TS)
		return WriteData(pBuffer, nSize);

    if (type == PLAYER_STREAMTYPE_VIDEO) {

        pcodec = vcodec;
        lp_lock(&mutex);
        codec_get_vbuf_state(pcodec, &video_buf);
        lp_unlock(&mutex);
        if(video_buf.size != 0)
            video_buf_level = (float)video_buf.data_len / video_buf.size;
        	//if (video_buf.data_len > 0x1000*1000) { //4M
        if (video_buf_level >= 0.9) {
            LOGI("SoftWriteData : video_buf.data_len=%d, timestamp=%lld", video_buf.data_len, timestamp);
            usleep(20*1000);
            return -1;
        } else {
            lp_lock(&mutex);
            if (timestamp >= 0 && timestamp != 0xFFFFFFFF) {
                codec_checkin_pts(pcodec,  timestamp);
            }
            lp_unlock(&mutex);
        }

    } else if (type == PLAYER_STREAMTYPE_AUDIO) {
        pcodec = acodec;

        // avoid some time not init audio
        if (acodec->has_audio == 0) {
            return nSize;
        }
	    lp_lock(&mutex);
        codec_get_abuf_state(pcodec, &audio_buf);
	    lp_unlock(&mutex);
	    if(audio_buf.size != 0)
            audio_buf_level = (float)audio_buf.data_len / audio_buf.size;
        	//if (audio_buf.data_len > 0x1000*250*2) { //2M
	    if (audio_buf_level >= 0.8) {
            LOGI("SoftWriteData : audio_buf.data_len=%d, timestamp=%lld", audio_buf.data_len, timestamp);
            usleep(20*1000);
            return -1;
        } else {
         	lp_lock(&mutex);
            if (timestamp >= 0) {
                codec_checkin_pts(pcodec,  timestamp);
            }
	        lp_unlock(&mutex);
        }

    } else {
        LOGI("SoftWriteData : type=%d, unknow type,return -1\n", type);
        return -1;
    }
    //LOGI("SoftWriteData: nSize=%d, type=%d,audio_buf.data_len: %d, video_buf.data_len: %d,timestamp=%lld\n",  nSize, type, audio_buf.data_len, video_buf.data_len, timestamp/90);

    switch(type) {
	case PLAYER_STREAMTYPE_AUDIO:
		pcodec = acodec;
		break;
	case PLAYER_STREAMTYPE_VIDEO:
		pcodec = vcodec;
		break;
	default:
		return nSize;
    }

    int temp_size = 0;
    for(int retry_count=0; retry_count<10; retry_count++) {
        lp_lock(&mutex);
        ret = codec_write(pcodec, pBuffer+temp_size, nSize-temp_size);
        lp_unlock(&mutex);
        if((ret < 0) || (ret > nSize)) {
            if((ret < 0) && (errno== EAGAIN)) {
                usleep(10);
                LOGI("SoftWriteData: codec_write return EAGAIN!\n");
                continue;
            } else {
                LOGI("SoftWriteData: codec_write return %d!\n", ret);
                if(pcodec->handle > 0){
                    lp_lock(&mutex);
                    ret = codec_close(pcodec);
		            //ALOGD("close and init");
                    ret = codec_init(pcodec);
                    lp_unlock(&mutex);
                    LOGI("SoftWriteData : codec need close and reinit m_bFast=%d\n", m_bFast);
                } else {
                    LOGI("SoftWriteData: codec_write return error or stop by called!\n");
                    break;
                }
            }
        } else {
            temp_size += ret;
            //LOGI("WriteData : codec_write  nSize is %d! temp_size=%d retry_count=%d\n", nSize, temp_size, retry_count);
            if(temp_size >= nSize) {
                temp_size = nSize;
                break;
            }
	    // release 10ms to other thread, for example decoder and drop pcm
            usleep(5000);
        }
    }
	if (ret >= 0 && temp_size > ret)
        ret = temp_size;

    if(ret > 0) {
    } else {
        return -1;
    }
    return ret;
}


int CTsPlayer::WriteData(unsigned char* pBuffer, unsigned int nSize)
{
    int ret = -1;
    int temp_size = 0;
    static int retry_count = 0;
    buf_status audio_buf;
    buf_status video_buf;
    float audio_buf_level = 0.00f;
    float video_buf_level = 0.00f;

    if(!m_bIsPlay || (m_bchangeH264to4k && !s_h264sameucode))
        return -1;

    LOGI("--WriteData, nSize=%d, prop_softdemux=%d--\n", nSize, prop_softdemux);

    //checkBuffstate();
    if(prop_softdemux == 0){
        codec_get_abuf_state(pcodec, &audio_buf);
        codec_get_vbuf_state(pcodec, &video_buf);
    }
    if(audio_buf.size != 0)
        audio_buf_level = (float)audio_buf.data_len / audio_buf.size;
    if(video_buf.size != 0)
        video_buf_level = (float)video_buf.data_len / video_buf.size;

    if(prop_softdemux == 0){
        if((audio_buf_level >= MAX_WRITE_ALEVEL) || (video_buf_level >= MAX_WRITE_VLEVEL)) {
            LOGI("WriteData : audio_buf_level= %.5f, video_buf_level=%.5f, Don't writedate()\n", audio_buf_level, video_buf_level);
            return -1;
        }
    }

    if(prop_softdemux == 1){
        int ret = write(pipe_fd[1], pBuffer, nSize);
        if (m_fp != NULL) {
            fwrite(pBuffer, 1, nSize, m_fp);
        }
        if(ret <= 0)
            LOGI("pipe is full,Don't write\n");
        else
            m_sCtsplayerState.bytes_record_cur += ret;
        return ret;
    }
    lp_lock(&mutex);

    if ((pcodec->video_type == VFORMAT_H264) && !s_h264sameucode && lpbuffer_st.enlpflag) {
        lp_lock(&mutex_lp);
        if (lpbuffer_st.wp + nSize < lpbuffer_st.bufferend) {
            lpbuffer_st.wp = (unsigned char *)memcpy(lpbuffer_st.wp, pBuffer, nSize);
            lpbuffer_st.wp += nSize;
            lpbuffer_st.valid_can_read += nSize;
        } else {
            lpbuffer_st.wp = lpbuffer_st.buffer;
            lpbuffer_st.enlpflag = false;
            LOGD("Don't use lpbuffer enlpflag:%d\n", lpbuffer_st.enlpflag);
            free(lpbuffer_st.buffer);
            lpbuffer_st.buffer = NULL;
            lpbuffer_st.rp = NULL;
            lpbuffer_st.wp = NULL;
            lpbuffer_st.bufferend = NULL;
            lpbuffer_st.enlpflag = 0;
            lpbuffer_st.valid_can_read = 0;
        }
        lp_unlock(&mutex_lp);

        LOGD("lpbuffer_st.valid_can_read:%d\n", lpbuffer_st.valid_can_read);

        for(int retry_count=0; retry_count< 50; retry_count++) {
            ret = codec_write(pcodec, pBuffer+temp_size, nSize-temp_size);
            if((ret < 0) || (ret > nSize)) {
                if(ret < 0 && errno == EAGAIN) {
                    usleep(10);
                    LOGI("WriteData: codec_write return EAGAIN!\n");
                    continue;
                }
            } else {
                temp_size += ret;
                LOGD("WriteData: codec_write h264 nSize is %d! temp_size=%d\n", nSize, temp_size);
                if(temp_size >= nSize) {
                    temp_size = nSize;
                    break;
                }
            }
        }
        if (ret >= 0 && temp_size > ret)
            ret = temp_size; // avoid write size large than 64k size
    } else {
        for(int retry_count=0; retry_count<50; retry_count++) {
            ret = codec_write(pcodec, pBuffer+temp_size, nSize-temp_size);
            if((ret < 0) || (ret > nSize)) {
                if(ret < 0 && (errno == EAGAIN)) {
                    usleep(10);
                    LOGI("WriteData: codec_write return EAGAIN!\n");
                    continue;
                } else {
                    LOGI("WriteData: codec_write return %d!\n", ret);
                    if(pcodec->handle > 0){
                        ret = codec_close(pcodec);
                        ret = codec_init(pcodec);
                        if(m_bFast) {
                            codec_set_mode(pcodec, TRICKMODE_I);
                        }
                        LOGI("WriteData : codec need close and reinit m_bFast=%d\n", m_bFast);
                    } else {
                        LOGI("WriteData: codec_write return error or stop by called!\n");
                        break;
                    }
                }
            } else {
                temp_size += ret;

                if(prop_write_log == 1)
                    LOGI("WriteData: codec_write  nSize is %d! temp_size=%d retry_count=%d\n", nSize, temp_size, retry_count);

                if(temp_size >= nSize) {
                    temp_size = nSize;
                    break;
                }
                // release 10ms to other thread, for example decoder and drop pcm
                usleep(2000);
            }
        }
        if (ret >= 0 && temp_size > ret)
            ret = temp_size; // avoid write size large than 64k size
    }
    lp_unlock(&mutex);

    if(ret > 0) {
		if (temp_size > 0) {
			m_sCtsplayerState.bytes_record_cur += temp_size;
		}

        if((m_fp != NULL) && (temp_size > 0)) {
            fwrite(pBuffer, 1, temp_size, m_fp);
            LOGI("ret[%d] temp_size[%d] nSize[%d]\n", ret, temp_size, nSize);
        }
        if(writecount >= MAX_WRITE_COUNT) {
            m_bWrFirstPkg = false;
            writecount = 0;
        }

        if(m_bWrFirstPkg == true) {
            writecount++;
        }
    } else {
        LOGW("WriteData: codec_write fail(%d),temp_size[%d] nSize[%d]\n", ret, temp_size, nSize);
		if(temp_size > 0) {
			if(m_fp != NULL)
				fwrite(pBuffer, 1, temp_size, m_fp);
			return temp_size;
		}
        return -1;
    }
    return ret;
}

bool CTsPlayer::Pause()
{
    m_bIsPause = true;
    codec_pause(pcodec);
    return true;
}

bool CTsPlayer::Resume()
{
    m_bIsPause = false;
    codec_resume(pcodec);
    return true;
}

static void Check_FirstPictur_Coming(void)
{
    if(get_sysfs_int("/sys/module/amvideo/parameters/first_frame_toggled")){
       if(perform_flag){
            amsysfs_set_sysfs_str(CPU_SCALING_MODE_NODE,DEFAULT_MODE);
            perform_flag =0;
        }
    }
}

bool CTsPlayer::Fast()
{
    int ret;

    LOGI("Fast");

    amsysfs_set_sysfs_int("/sys/module/amvideo/parameters/chip_fast_flag", 1);

    if(m_bFast){
      LOGI("Last is Fast");
      return true;
    }
    ret = amsysfs_set_sysfs_int("/sys/class/video/blackout_policy", 0);
    if(ret)
        return false;
    keep_vdec_mem = 1;
    if(pcodec->video_type == VFORMAT_HEVC) {
        amsysfs_set_sysfs_int("/sys/module/amvdec_h265/parameters/buffer_mode", 1);
    }
    iStop();
    m_bFast = true;

    // remove di from vfm path
    //remove_di();

    //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 1);
    //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_trick_mode", 2);
    amsysfs_set_sysfs_int("/sys/module/di/parameters/start_frame_drop_count",0);

    ret = iStartPlay();
    if(!ret)
        return false;

    LOGI("Fast: codec_set_mode: %d\n", pcodec->handle);
    amsysfs_set_sysfs_int("/sys/class/tsync/enable", 0);
    codec_set_freerun_mode(pcodec, 1);
    if(pcodec->video_type == VFORMAT_HEVC){
        if(prop_softdemux == 0)
            ret = codec_set_mode(pcodec, TRICKMODE_I_HEVC);
        else
            ret = codec_set_mode(vcodec, TRICKMODE_I_HEVC);
    } else {
        if(prop_softdemux == 0)
            ret = codec_set_mode(pcodec, TRICKMODE_I);
        else
            ret = codec_set_mode(vcodec, TRICKMODE_I);
    }
    return !ret;
}

bool CTsPlayer::StopFast()
{
    int ret;

    amsysfs_set_sysfs_int("/sys/module/amvideo/parameters/chip_fast_flag", 0);

    if(!m_bFast){
      LOGI("Last is None fast");
      return true;
    }

    LOGI("StopFast");
    if (pcodec->has_sub == 1)
        subtitleResetForSeek();
    m_bFast = false;

    ret=codec_set_freerun_mode(pcodec, 0);
    if(ret){
        LOGI("error stopfast set freerun_mode 0 fail\n");
    }

    if(prop_softdemux == 0)
        ret = codec_set_mode(pcodec, TRICKMODE_NONE);
    else
        ret = codec_set_mode(vcodec, TRICKMODE_NONE);
    //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 0);
    keep_vdec_mem = 1;
    iStop();
    //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 0);
    //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_trick_mode", 1);
    amsysfs_set_sysfs_int("/sys/class/tsync/enable", 1);

    ret = iStartPlay();
    if(!ret)
        return false;
    if (!m_isBlackoutPolicy) {
        ret = amsysfs_set_sysfs_int("/sys/class/video/blackout_policy", 1);
        if (ret)
            return false;
    }

    return true;
}

bool CTsPlayer::Stop(){
    int ret;

    amsysfs_set_sysfs_int("/sys/module/amvideo/parameters/ctsplayer_exist", 0);
    if (!m_bIsPlay) {
        LOGI("already is Stoped\n");
        return true;
    }

    codec_set_freerun_mode(pcodec, 0);
    if (pcodec->has_sub == 1) {
        memset(sPara,0,sizeof(SUBTITLE_PARA_T)*MAX_SUBTITLE_PARAM_SIZE);
    }

    if (pcodec->video_type == VFORMAT_HEVC) {
        amsysfs_set_sysfs_int("/sys/module/amvdec_h265/parameters/buffer_mode", 8);
    }

    ret = iStop();

    adec_underflow = 0;
    vdec_underflow = 0;

    return ret;
}

bool CTsPlayer::iStop()
{
    int ret;

    LOGI("Stop keep_vdec_mem: %d\n", keep_vdec_mem);
    amsysfs_set_sysfs_int("/sys/class/vdec/keep_vdec_mem", keep_vdec_mem);
    amsysfs_set_sysfs_int("/sys/module/di/parameters/start_frame_drop_count",2);
    amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/error_skip_divisor", 0);
    if (perform_flag) {
        amsysfs_set_sysfs_str(CPU_SCALING_MODE_NODE,DEFAULT_MODE);
        perform_flag =0;
    }
    if (m_bIsPlay) {
        LOGI("m_bIsPlay is true");
        if (m_fp != NULL) {
            fclose(m_fp);
            m_fp = NULL;
        }

        //amsysfs_set_sysfs_int("/sys/module/di/parameters/bypass_all", 0);
        if (prop_softdemux == 1 && prop_esdata != 1) {
            uint8_t *tmp_buf = (uint8_t *)malloc(1024*32);
            if (tmp_buf == NULL) {
                LOGE("malloc tmp_buf failed");
                return false;
            }
            close(pipe_fd[1]);
            while (read(pipe_fd[0], tmp_buf, 1024*32)>0);
                free(tmp_buf);
            close(pipe_fd[0]);
            LOGI("pipe closed first");
        }
        lp_lock(&mutex);
        if (m_bIsPlay == false) {
            LOGI("Already stop return\n");
            lp_unlock(&mutex);
            return true;//avoid twice stop
        }
        m_bFast = false;
        m_bIsPlay = false;
        m_bIsPause = false;
        m_StartPlayTimePoint = 0;
        m_PreviousOverflowTime = 0;
        if (prop_softdemux == 0) {
            ret = codec_set_mode(pcodec, TRICKMODE_NONE);
            ret = codec_close(pcodec);
            pcodec->handle = -1;
        } else {
            ret = codec_set_mode(vcodec, TRICKMODE_NONE);

            if(acodec != NULL){
                ret = codec_close(acodec);
                if (ret < 0) {
                    lp_unlock(&mutex);
                    LOGI("[es_release]close acodec failed, ret= %x\n", ret);
                    return ret;
                }
            }
            if (vcodec) {
                ret = codec_close(vcodec);
                if (ret < 0) {
                    lp_unlock(&mutex);
                    LOGI("[es_release]close vcodec failed, ret= %x\n", ret);
                    return ret;
                }
            }
#if 1
            if(prop_esdata != 1) {
                am_ffextractor_deinit();
                am_ffextractor_inited = false;
            }
#endif
            LOGI("ffmpeg denited finally");
        }
        //check_add_ppmgr();
        LOGI("Stop  codec_close After:%d\n", ret);
#ifdef USE_OPTEEOS
        char vaule[PROPERTY_VALUE_MAX] = {0};
        int tvpdrm = 1;
        memset(vaule, 0, PROPERTY_VALUE_MAX);
        property_get("iptv.tvpdrm", vaule, "1");
        tvpdrm = atoi(vaule);
        LOGI("prop_tvpdrm :%d, 1 tvp and 0 is no tvp debug \n",tvpdrm);
        if ((tvpdrm == 1) && (prop_softdemux == 1)) {
            PA_Getsecmem(0);
        }
#endif
        m_bWrFirstPkg = true;
        //add_di();
        if (pcodec->has_sub == 1)
            subtitleClose();
        if (!s_h264sameucode && lpbuffer_st.buffer != NULL){
            lp_lock(&mutex_lp);
            free(lpbuffer_st.buffer);
            lpbuffer_st.buffer = NULL;
            lpbuffer_st.rp = NULL;
            lpbuffer_st.wp = NULL;
            lpbuffer_st.bufferend = NULL;
            lpbuffer_st.enlpflag = 0;
            lpbuffer_st.valid_can_read = 0;
            lp_unlock(&mutex_lp);
       }
       lp_unlock(&mutex);
    } else {
        LOGI("m_bIsPlay is false");
    }
    if (NULL != pcodec && pcodec->has_sub == 1)
        mSubRatioThreadStop = true;

    return true;
}

bool CTsPlayer::Seek()
{
    LOGI("Seek");
    int ret = 0;
    if(pcodec->video_type == VFORMAT_HEVC) {
        amsysfs_set_sysfs_int("/sys/module/amvdec_h265/parameters/buffer_mode", 1);
    }
    ret = codec_set_freerun_mode(pcodec, 0);
    if (ret) {
        LOGI("error seek set freerun_mode 0 fail\n");
    }
    iStop();
    //usleep(500*1000);
    iStartPlay();
    return true;
}
static float last_vol = 1.0;
static int hasSetVolume = -1;
static int get_android_stream_volume(float *volume)
{
    float vol = last_vol;
    unsigned int sr = 0;

    AudioSystem::getOutputSamplingRate(&sr,AUDIO_STREAM_MUSIC);
    if(sr > 0){
        audio_io_handle_t handle = -1;
        handle = AudioSystem::getOutput(AUDIO_STREAM_MUSIC,
                  48000,
                  AUDIO_FORMAT_PCM_16_BIT,
                  AUDIO_CHANNEL_OUT_STEREO,
                  AUDIO_OUTPUT_FLAG_PRIMARY
                  );

        if(handle > 0){
            if(AudioSystem::getStreamVolume(AUDIO_STREAM_MUSIC,&vol,handle) == 	NO_ERROR){
            	*volume = vol;
                if((last_vol!=vol)||(hasSetVolume==-1)){
                	last_vol = vol;
                	hasSetVolume=-1;
                	return 0;
                }

                return 0;
            }
            else
                LOGI("get stream volume failed\n");
         }
         else
            LOGI("get output handle failed\n");
    }
    return -1;
}

static int set_android_stream_volume(float volume)
{
	unsigned int sr = 0;

	AudioSystem::getOutputSamplingRate(&sr,AUDIO_STREAM_MUSIC);
	if(sr > 0){
		audio_io_handle_t handle = -1;
		handle = AudioSystem::getOutput(AUDIO_STREAM_MUSIC,
				48000,
				AUDIO_FORMAT_PCM_16_BIT,
				AUDIO_CHANNEL_OUT_STEREO,
				AUDIO_OUTPUT_FLAG_PRIMARY
				);

		if(handle > 0){
			if (AudioSystem::setStreamVolume(AUDIO_STREAM_MUSIC,volume, handle) == 	NO_ERROR) {
				LOGI("set stream volume suc\n");
				return 0;
			}
			LOGI("set stream volume failed\n");
		}
		else
			LOGI("set output handle failed\n");
	}

    return -1;
}

int CTsPlayer::GetVolume()
{
    float volume = 1.0f;

    int ret = get_android_stream_volume(&volume);
    LOGI("GetVolume: volume:%f , ret:%d \n",volume,ret);

    if(ret < 0 || volume<0) {
        return m_nVolume;
    }

	m_nVolume = (int)(volume*100);
    return (int)(volume*100);
}

bool CTsPlayer::SetVolume(int volume)
{
    LOGI("SetVolume");
    if(volume<0 || volume>100){
        LOGI("SetVolume , value is invalid \n");
        return false;
    }
    //int ret = codec_set_volume(pcodec, (float)volume/100.0);
    int ret = set_android_stream_volume((float)volume/100.0);
    m_nVolume = volume;
    hasSetVolume=1;
    return true;
}

//get current sound track
//return parameter: 1, Left Mono; 2, Right Mono; 3, Stereo; 4, Sound Mixing
int CTsPlayer::GetAudioBalance()
{
    return m_nAudioBalance;
}

//set sound track
//input paramerter: nAudioBlance, 1, Left Mono; 2, Right Mono; 3, Stereo; 4, Sound Mixing
bool CTsPlayer::SetAudioBalance(int nAudioBalance)
{
    if((nAudioBalance < 1) && (nAudioBalance > 4))
        return false;
    m_nAudioBalance = nAudioBalance;
    if(nAudioBalance == 1) {
        LOGI("SetAudioBalance 1 Left Mono\n");
        //codec_left_mono(pcodec);
        codec_lr_mix_set(pcodec, 0);
         amsysfs_set_sysfs_str("/sys/class/amaudio/audio_channels_mask", "l");
    } else if(nAudioBalance == 2) {
        LOGI("SetAudioBalance 2 Right Mono\n");
        //codec_right_mono(pcodec);
        codec_lr_mix_set(pcodec, 0);
        amsysfs_set_sysfs_str("/sys/class/amaudio/audio_channels_mask", "r");
    } else if(nAudioBalance == 3) {
        LOGI("SetAudioBalance 3 Stereo\n");
        //codec_stereo(pcodec);
        codec_lr_mix_set(pcodec, 0);
        amsysfs_set_sysfs_str("/sys/class/amaudio/audio_channels_mask", "s");
    } else if(nAudioBalance == 4) {
        LOGI("SetAudioBalance 4 Sound Mixing\n");
        //codec_stereo(pcodec);
        codec_lr_mix_set(pcodec, 1);
        //amsysfs_set_sysfs_str("/sys/class/amaudio/audio_channels_mask", "c");
    }
    return true;
}

void CTsPlayer::GetVideoPixels(int& width, int& height)
{
    int x = 0, y = 0;
    OUTPUT_MODE output_mode = get_display_mode();
    getPosition(output_mode, &x, &y, &width, &height);
    LOGI("GetVideoPixels, x: %d, y: %d, width: %d, height: %d", x, y, width, height);
}

bool CTsPlayer::SetRatio(int nRatio)
{
    char writedata[40] = {0};
    int width = 0;
    int height = 0;
    int new_x = 0;
    int new_y = 0;
    int new_width = 0;
    int new_height = 0;
    int mode_x = 0;
    int mode_y = 0;
    int mode_width = 0;
    int mode_height = 0;
    vdec_status vdec;
    if(!m_bIsPlay){
        return false;
    }
    if (prop_softdemux == 0) {
        codec_get_vdec_state(pcodec,&vdec);
    } else {
        codec_get_vdec_state(vcodec,&vdec);
    }
    width = vdec.width;
    height = vdec.height;

    LOGI("SetRatio width: %d, height: %d, nRatio: %d\n", width, height, nRatio);
    OUTPUT_MODE output_mode = get_display_mode();
    getPosition(output_mode, &mode_x, &mode_y, &mode_width, &mode_height);

    if((nRatio != 255) && (amsysfs_get_sysfs_int("/sys/class/video/disable_video") == 1))
        amsysfs_set_sysfs_int("/sys/class/video/disable_video", 2);
    if(nRatio == 1) {	 //Full screen
        new_x = mode_x;
        new_y = mode_y;
        new_width = mode_width;
        new_height = mode_height;
        sprintf(writedata, "%d %d %d %d", new_x, new_y, new_x +new_width - 1, new_y+new_height - 1);
        amsysfs_set_sysfs_str("/sys/class/video/axis", writedata);
        return true;
    } else if(nRatio == 2) {	//Fit by width
        new_width = mode_width;
        new_height = int(mode_width*height/width);
        new_x = mode_x;
        new_y = mode_y + int((mode_height-new_height)/2);
        LOGI("SetRatio new_x: %d, new_y: %d, new_width: %d, new_height: %d\n"
                , new_x, new_y, new_width, new_height);
        sprintf(writedata, "%d %d %d %d", new_x, new_y, new_x+new_width-1, new_y+new_height-1);
        amsysfs_set_sysfs_str("/sys/class/video/axis",writedata);
        return true;
    } else if(nRatio == 3) {	//Fit by height
        new_width = int(mode_height*width/height);
        new_height = mode_height;
        new_x = mode_x + int((mode_width - new_width)/2);
        new_y = mode_y;
        LOGI("SetRatio new_x: %d, new_y: %d, new_width: %d, new_height: %d\n"
                , new_x, new_y, new_width, new_height);
        sprintf(writedata, "%d %d %d %d", new_x, new_y, new_x+new_width-1, new_y+new_height-1);
        amsysfs_set_sysfs_str("/sys/class/video/axis", writedata);
        return true;
    } else if(nRatio == 255) {
        amsysfs_set_sysfs_int("/sys/class/video/disable_video", 1);
        return true;
    }
    return false;
}

bool CTsPlayer::IsSoftFit()
{
    return m_isSoftFit;
}

void CTsPlayer::SetEPGSize(int w, int h)
{
    LOGI("SetEPGSize: w=%d, h=%d, m_bIsPlay=%d\n", w, h, m_bIsPlay);
    m_nEPGWidth = w;
    m_nEPGHeight = h;
    if(!m_isSoftFit && !m_bIsPlay){
        InitOsdScale(m_nEPGWidth, m_nEPGHeight);
    }
}

void CTsPlayer::readExtractor() {
	int size = 0;
	int index = -1;
    int temp_size = 0;
	int64_t pts = 0;
	int ret = 0;
	if (am_ffextractor_inited == false) {
		MediaInfo mi;
		int try_count = 0;
		while (try_count++ < 3) {
			LOGD("try to init ffmepg %d times", try_count);
			int ret = am_ffextractor_init(read_cb, &mi);
			if (ret == -1) {
				LOGD("ffextractor_init return -1");
				continue;
			}
			if (mi.width ==0 || mi.height == 0) {
				LOGD("invalid dimensions: %d:%d", mi.width, mi.height);
			}
			am_ffextractor_inited = true;
			break;
		}
	}

	if (am_ffextractor_inited == false) {
		LOGE("3 Attempts to init ffextractor all failed");
	//	mForceStop = true;
		return;
	}

	lp_lock(&mutex);
	if(m_bIsPlay == false){
		LOGI("now in stop state,return\n");
        lp_unlock(&mutex);
       	return;
   	}
	am_ffextractor_read_packet(vcodec, acodec);
	lp_unlock(&mutex);
}

void CTsPlayer::SwitchAudioTrack(int pid)
{
    int count = 0;

    while ((a_aPara[count].pid != pid) &&(a_aPara[count].pid != 0)
            &&(count < MAX_AUDIO_PARAM_SIZE)) {
        count++;
    }

    if (!m_bIsPlay) {
        LOGE("Player is not on play status!\n");
        return;
    }
    if (count >= MAX_AUDIO_PARAM_SIZE) {
        LOGE("count(%d) over MAX_AUDIO_PARAM_SIZE!\n", count);
        return;
    }
    if (pcodec->audio_pid == (int)a_aPara[count].pid) {
        LOGE("Same audio pid(%d), do not change!\n", pcodec->audio_pid);
        return;
    }

    lp_lock(&mutex);
    codec_audio_automute(pcodec->adec_priv, 1);
    codec_close_audio(pcodec);
    pcodec->audio_pid = 0xffff;

    if (codec_set_audio_pid(pcodec)) {
        LOGE("set invalid audio pid failed\n");
        lp_unlock(&mutex);
        return;
    }

    pcodec->has_audio = 1;
    pcodec->audio_type = a_aPara[count].aFmt;
    pcodec->audio_pid = (int)a_aPara[count].pid;
    LOGI("SwitchAudioTrack pcodec->audio_samplerate: %d, pcodec->audio_channels: %d\n", pcodec->audio_samplerate, pcodec->audio_channels);
    LOGI("SwitchAudioTrack pcodec->audio_type: %d, pcodec->audio_pid: %d\n", pcodec->audio_type, pcodec->audio_pid);

    //codec_set_audio_pid(pcodec);
    if (IS_AUIDO_NEED_EXT_INFO(pcodec->audio_type)) {
        pcodec->audio_info.valid = 1;
        LOGI("set audio_info.valid to 1");
    }

    if (codec_audio_reinit(pcodec)) {
        LOGE("reset init failed\n");
        lp_unlock(&mutex);
        return;
    }

    if (codec_reset_audio(pcodec)) {
        LOGE("reset audio failed\n");
        lp_unlock(&mutex);
        return;
    }
    codec_resume_audio(pcodec, 1);
    codec_audio_automute(pcodec->adec_priv, 0);
    lp_unlock(&mutex);
}

void CTsPlayer::ClearLastFrame()
{
    int ret = -1;
    LOGI( "Begin CTsPlayer::ClearLastFrame().\n");
    amsysfs_set_sysfs_int( "/sys/class/video/disable_video", 1);
    int fd_fb0 = open("/dev/amvideo", O_RDWR);
    if (fd_fb0 >= 0)
    {
        ret = ioctl(fd_fb0, AMSTREAM_IOC_CLEAR_VBUF, NULL);
        ret = ioctl(fd_fb0, AMSTREAM_IOC_CLEAR_VIDEO, NULL);
        close(fd_fb0);
    }
    amsysfs_set_sysfs_int( "/sys/class/video/disable_video", 0);

    LOGI( "End  CTsPlayer::ClearLastFrame().\n");
    return;
}

void CTsPlayer::BlackOut(int EarseLastFrame)
{
    LOGI( "Begin CTsPlayer::BlackOut().EarseLastFrame=%d.\n", EarseLastFrame);
    amsysfs_set_sysfs_int( "/sys/class/video/blackout_policy", EarseLastFrame);
    LOGI( "End   CTsPlayer::BlackOut().\n");
    return;
}

bool CTsPlayer::SetErrorRecovery(int mode)
{
    LOGI( "This is CTsPlayer::SetErrorRecovery(). mode=%d.\n", mode);

    if((mode >= 0)&&(mode <= 3))
    {
        amsysfs_set_sysfs_int( "/sys/module/amvdec_h264/parameters/error_recovery_mode", mode);
        amsysfs_set_sysfs_int( "/sys/module/amvdec_mpeg12/parameters/error_frame_skip_level", mode);//modifier by chengman for mpeg errorrecovery 3-15
    }
    else
    {
        return false;
    }

    return true;
}

void CTsPlayer::GetAvbufStatus(PAVBUF_STATUS pstatus)
{
	buf_status audio_buf;
	buf_status video_buf;

	//LOGI( "This is CTsPlayer::GetAvbufStatus(). pstatus=%d.\n", pstatus);

	if(pstatus == NULL)
	{
		return;
	}

	codec_get_abuf_state(pcodec,&audio_buf);
	codec_get_vbuf_state(pcodec,&video_buf);

	pstatus->abuf_size = audio_buf.size;
	pstatus->abuf_data_len = audio_buf.data_len;
	pstatus->abuf_free_len = audio_buf.free_len;
	pstatus->vbuf_size = video_buf.size;
	pstatus->vbuf_data_len = video_buf.data_len;
	pstatus->vbuf_free_len = video_buf.free_len;

	return;
}

void CTsPlayer::SwitchSubtitle(int pid)
{
    LOGI("SwitchSubtitle be called pid is %d\n", pid);
    if (pcodec->has_sub == 1)
        subtitleResetForSeek();
    /* first set an invalid sub id */
    pcodec->sub_pid = 0xffff;
    if(codec_set_sub_id(pcodec)) {
        LOGE("[%s:%d]set invalid sub pid failed\n", __FUNCTION__, __LINE__);
        return;
    }
    int count=0;
    PSUBTITLE_PARA_T pSubtitlePara=sPara;
    while((pSubtitlePara->pid != 0) && (count < MAX_SUBTITLE_PARAM_SIZE)) {
        if(pSubtitlePara->pid == pid){
            setSubType(pSubtitlePara);
            break;
        }
        count++;
        pSubtitlePara++;
    }
    /* reset sub */
    pcodec->sub_pid = pid;
    if(codec_set_sub_id(pcodec)) {
        LOGE("[%s:%d]set invalid sub pid failed\n", __FUNCTION__, __LINE__);
        return;
    }

    if(codec_reset_subtile(pcodec)) {
        LOGE("[%s:%d]reset subtile failed\n", __FUNCTION__, __LINE__);
    }
}

bool CTsPlayer::SubtitleShowHide(bool bShow)
{
    LOGV("[%s:%d]\n", __FUNCTION__, __LINE__);
    if (pcodec->has_sub == 1) {
        if (bShow) {
            subtitleDisplay();
        } else {
            subtitleHide();
        }
    } else {
        LOGV("[%s:%d]No subtitle !\n", __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}

void CTsPlayer::SetProperty(int nType, int nSub, int nValue)
{

}

int64_t CTsPlayer::GetCurrentPlayTime()
{
    int64_t video_pts = 0;
    unsigned long audiopts = 0;
    unsigned long videopts = 0;
    unsigned long pcrscr = 0;
    unsigned long checkin_vpts = 0;

    unsigned int tmppts = 0;
    if (m_bIsPlay){
    	if ((pcodec->video_type == VFORMAT_HEVC) &&(m_bFast == true)) {
            tmppts = amsysfs_get_sysfs_int("/sys/module/amvdec_h265/parameters/h265_lookup_vpts");
            //LOGI("Fast: i only getvpts by h265_lookup_vpts :%d\n",tmppts);
    	}
    	else{
            tmppts = codec_get_vpts(pcodec);
    	}
    }
    video_pts = tmppts;
    if(m_bFast && (pcodec->video_type != VFORMAT_HEVC)){
        sysfs_get_long("/sys/class/tsync/pts_audio",&audiopts);
        sysfs_get_long("/sys/class/tsync/pts_video",&videopts);
        sysfs_get_long("/sys/class/tsync/pts_pcrscr",&pcrscr);
        LOGI("apts:0x%x,vpts=0x%x,pcrscr=0x%x\n",audiopts,videopts,pcrscr);
        sysfs_get_long("/sys/class/tsync/checkin_vpts",&checkin_vpts);
        LOGI("In Fast last checkin_vpts=0x%x\n",checkin_vpts);
        video_pts = (int64_t)checkin_vpts;
    }
    return video_pts;
}

void CTsPlayer::leaveChannel()
{
    LOGI("leaveChannel be call\n");
    Stop();
}

void CTsPlayer::SetSurface(Surface* pSurface)
{
    LOGI("SetSurface pSurface: %p\n", pSurface);
    amsysfs_set_sysfs_int("/sys/module/amvideo/parameters/ctsplayer_exist", 1);
    sp<IGraphicBufferProducer> mGraphicBufProducer;

    mGraphicBufProducer = pSurface->getIGraphicBufferProducer();
    if(mGraphicBufProducer != NULL) {
        mNativeWindow = new Surface(mGraphicBufProducer);
    } else {
        LOGE("SetSurface, mGraphicBufProducer is NULL!\n");
        return;
    }
    native_window_set_buffer_count(mNativeWindow.get(), 4);
    native_window_set_usage(mNativeWindow.get(), GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP | GRALLOC_USAGE_AML_VIDEO_OVERLAY);
    //native_window_set_buffers_format(mNativeWindow.get(), WINDOW_FORMAT_RGBA_8888);
    native_window_set_buffers_format(mNativeWindow.get(), WINDOW_FORMAT_RGB_565);
}
void CTsPlayer::update_nativewindow() {
    ANativeWindowBuffer* buf;
    char* vaddr;
	/*mNativeWindow->query(mNativeWindow.get(),NATIVE_WINDOW_WIDTH,&width_new);
	mNativeWindow->query(mNativeWindow.get(),NATIVE_WINDOW_HEIGHT,&height_new);
	if (width_old == width_new && height_old == height_new)
		return;
	width_old = width_new;
	height_old = height_new;*/
    int err = mNativeWindow->dequeueBuffer_DEPRECATED(mNativeWindow.get(), &buf);
    if (err != 0) {
        ALOGE("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
        return;
    }
    mNativeWindow->lockBuffer_DEPRECATED(mNativeWindow.get(), buf);
    sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buf, false));
    graphicBuffer->lock(1, (void **)&vaddr);
    //if (vaddr != NULL) {
    //    memset(vaddr, 0x0, graphicBuffer->getWidth() * graphicBuffer->getHeight() * 4); /*to show video in osd hole...*/
   // }
    graphicBuffer->unlock();
    graphicBuffer.clear();
    ALOGV("checkBuffLevel___queueBuffer_DEPRECATED");

    mNativeWindow->queueBuffer_DEPRECATED(mNativeWindow.get(), buf);

}


void CTsPlayer::playerback_register_evt_cb(IPTV_PLAYER_EVT_CB pfunc, void *hander)
{
    pfunc_player_evt = pfunc;
    player_evt_hander = hander;
}

void CTsPlayer::RegisterParamEvtCb(void *hander, IPTV_PLAYER_PARAM_Evt_e enEvt, IPTV_PLAYER_PARAM_EVENT_CB  pfunc)
{
    pfunc_player_param_evt = pfunc;
    LOGI("RegisterParamEvtCb pfunc: %p\n", pfunc);
    player_evt_param_handler = hander;

}

void CTsPlayer::checkBuffLevel()
{
	int audio_delay=0, video_delay=0;
    float audio_buf_level = 0.00f, video_buf_level = 0.00f;
    buf_status audio_buf;
    buf_status video_buf;

    if(m_bIsPlay) {
	#if 0
        codec_get_abuf_state(pcodec, &audio_buf);
        codec_get_vbuf_state(pcodec, &video_buf);
        if(audio_buf.size != 0)
            audio_buf_level = (float)audio_buf.data_len / audio_buf.size;
        if(video_buf.size != 0)
            video_buf_level = (float)video_buf.data_len / video_buf.size;
	#else
		codec_get_audio_cur_delay_ms(pcodec, &audio_delay);
		codec_get_video_cur_delay_ms(pcodec, &video_delay);
	#endif

        if(!m_bFast && m_StartPlayTimePoint > 0 && (((av_gettime() - m_StartPlayTimePoint)/1000 >= prop_buffertime)
                || (audio_delay >= prop_audiobuftime || video_delay >= prop_videobuftime))) {
            LOGI("av_gettime()=%lld, m_StartPlayTimePoint=%lld, prop_buffertime=%d\n", av_gettime(), m_StartPlayTimePoint, prop_buffertime);
            LOGI("audio_delay=%d, prop_audiobuftime=%d, video_delay=%d, prop_videobuftime=%d\n", audio_delay, prop_audiobuftime, video_delay, prop_videobuftime);
            LOGI("WriteData: resume play now!\n");
            codec_resume(pcodec);
            m_StartPlayTimePoint = 0;
        }
    }
}

void CTsPlayer::checkBuffstate()
{
    char filter_mode[PROPERTY_VALUE_MAX] = {0};
    struct vdec_status video_buf;
    if(m_bIsPlay) {
        if(prop_softdemux == 0)
            codec_get_vdec_state(pcodec, &video_buf);
        else
            codec_get_vdec_state(vcodec, &video_buf);
        if (video_buf.status & DECODER_ERROR_MASK) {
            LOGI("decoder error vdec.status: %x\n", video_buf.status);
            int is_decoder_fatal_error = video_buf.status & DECODER_FATAL_ERROR_SIZE_OVERFLOW;
            if(is_decoder_fatal_error && (pcodec->video_type == VFORMAT_H264)) {
                //change format  h264--> h264 4K
                keep_vdec_mem = 1;
                iStop();
                usleep(500*1000);
                if(property_get("ro.platform.filter.modes",filter_mode,NULL) ==  0){
                    vPara.vFmt = VFORMAT_H264_4K2K;
                    iStartPlay();
                    LOGI("start play vh264_4k2k");
                }
            }
        }
    }
}

void CTsPlayer::checkAbend()
{
    int ret = 0;
    buf_status audio_buf;
    buf_status video_buf;

    if(!m_bWrFirstPkg){
        bool checkAudio = true;
        codec_get_abuf_state(pcodec, &audio_buf);
        if(prop_softdemux == 0)
            codec_get_vbuf_state(pcodec, &video_buf);
        else
            codec_get_vbuf_state(vcodec, &video_buf);

        LOGI("checkAbend pcodec->video_type is %d, video_buf.data_len is %d\n", pcodec->video_type, video_buf.data_len);
        if(pcodec->has_video) {
            if(pcodec->video_type == VFORMAT_MJPEG) {
                if(video_buf.data_len < (RES_VIDEO_SIZE >> 2)) {
                    vdec_underflow = 1;
                    checkAudio = false;
                    LOGW("checkAbend video low level\n");
                } else {
                    vdec_underflow = 0;
                }
            }
            else {
                if(video_buf.data_len< RES_VIDEO_SIZE) {
                    vdec_underflow = 1;
                    checkAudio = false;
                    LOGW("checkAbend video low level\n");
                } else {
                    vdec_underflow = 0;
                }
            }
        }
        LOGI("checkAbend audio_buf.data_len is %d\n", audio_buf.data_len);
        if(pcodec->has_audio && checkAudio) {
            if(audio_buf.data_len < RES_AUDIO_SIZE) {
                adec_underflow = 1;
                LOGW("checkAbend audio low level\n");
            } else {
                adec_underflow = 0;
            }
        }
    }
}

void CTsPlayer::update_caton_info(struct av_param_info_t * info)
{
        struct codec_quality_info *pquality_info = &m_sCtsplayerState.quality_info;

	if (m_sCtsplayerState.first_picture_comming == 0) {
		return ;
	}

        if(codec_get_upload(&info->av_info, pquality_info)) {


		//start check underflow
            if (pquality_info->unload_flag) {
			LOGI("caton start : underflow(%d -> %d)\n",
                m_sCtsplayerState.caton_start_underflow, pquality_info->unload_flag);

			m_sCtsplayerState.catoning = 1;
                m_sCtsplayerState.caton_times += 1;
                m_sCtsplayerState.caton_start_underflow = pquality_info->unload_flag;
			m_sCtsplayerState.caton_start_time = (int64_t)(av_gettime()/1000);
	} else {
		//already underflow caton
			//caton end
			LOGI("caton end : underflow(%d -> %d)\n",
                m_sCtsplayerState.caton_start_underflow, pquality_info->unload_flag);
			m_sCtsplayerState.catoning = 0;
				m_sCtsplayerState.caton_time +=
					(int64_t)(av_gettime()/1000) - m_sCtsplayerState.caton_start_time;
                m_sCtsplayerState.caton_start_underflow = 0;
				LOGI("caton_times:%d, caton_time:%d\n",
					m_sCtsplayerState.caton_times, m_sCtsplayerState.caton_time);
			}
		}
        if(codec_get_blurred_screen(&info->av_info, pquality_info)) {
	}

}

void CTsPlayer::update_stream_bitrate()
{
	int64_t curtime_ms = 0;

	curtime_ms = (int64_t)(av_gettime()/1000);

	if (m_sCtsplayerState.bytes_record_starttime_ms == 0) {
		m_sCtsplayerState.bytes_record_starttime_ms = curtime_ms;
	} else {
		if (curtime_ms - m_sCtsplayerState.bytes_record_starttime_ms >= 1000) {
			m_sCtsplayerState.stream_bitrate =
				(m_sCtsplayerState.bytes_record_cur - m_sCtsplayerState.bytes_record_start)* 8;
			m_sCtsplayerState.bytes_record_start = m_sCtsplayerState.bytes_record_cur;
			m_sCtsplayerState.bytes_record_starttime_ms = curtime_ms;
			LOGI("TsPlayer update writedata bitrate:%dbps\n", m_sCtsplayerState.stream_bitrate);
		}
	}
}
void CTsPlayer::checkVdecstate()
{
    float audio_buf_level = 0.00f, video_buf_level = 0.00f;
    buf_status audio_buf;
    buf_status video_buf;
    struct vdec_status video_status;
    int64_t  last_time, now_time;
    if(m_bIsPlay) {
        Check_FirstPictur_Coming();
        if(prop_softdemux == 0)
            codec_get_vdec_state(pcodec, &video_status);
        else
            codec_get_vdec_state(vcodec, &video_status);
        if (!s_h264sameucode && video_status.status & DECODER_ERROR_MASK) {
            LOGI("decoder vdec.status: %x width : %d height: %d\n", video_status.status, video_status.width, video_status.height);

            if((video_status.status & DECODER_FATAL_ERROR_SIZE_OVERFLOW) &&
               (pcodec->video_type == VFORMAT_H264) &&
               (video_status.width > 1920 || video_status.height > 1080)) {
                now_time = av_gettime();
                m_bchangeH264to4k = true;
                //change format  h264--> h264 4K
                LOGI("change format  h264--> h264 4K: %x width : %d height: %d\n",
                       video_status.status, video_status.width, video_status.height);

                LOGI("Begin start!!!! before rp:0x%x  wp:0x%x start:0x%x end:0x%x\n",
                      lpbuffer_st.rp, lpbuffer_st.wp, lpbuffer_st.buffer, lpbuffer_st.bufferend);
                codec_close(pcodec);
                vPara.vFmt = VFORMAT_H264_4K2K;
                pcodec->video_type = VFORMAT_H264_4K2K;
                pcodec->am_sysinfo.format = VIDEO_DEC_FORMAT_H264_4K2K;
                codec_init(pcodec);
                while(lpbuffer_st.valid_can_read > 0 && lpbuffer_st.enlpflag) {
                    int ret, temp_size = 0, can_write = READ_SIZE;
                    if(lpbuffer_st.valid_can_read < can_write) {
                        can_write = lpbuffer_st.valid_can_read;
                    }
                    for(int retry_count=0; retry_count<10; retry_count++) {
                        ret = codec_write(pcodec, lpbuffer_st.rp + temp_size, can_write - temp_size);
                        if((ret < 0) || (ret > can_write)) {
                            if(ret == EAGAIN) {
                                usleep(10);
                                LOGI("WriteData: codec_write return EAGAIN!\n");
                                continue;
                            }
                        } else {
                            temp_size += ret;
                            if(temp_size >= can_write) {
                                lp_lock(&mutex_lp);
                                lpbuffer_st.valid_can_read -= can_write;
                                lpbuffer_st.rp += can_write;
                                if(lpbuffer_st.rp >= lpbuffer_st.bufferend) {
                                    lpbuffer_st.enlpflag = false;
                                    lpbuffer_st.rp = lpbuffer_st.buffer;
                                }
                                lp_unlock(&mutex_lp);
                                break;
                            }
                        }
                    }
                    //LOGI("valid_can_read : %d\n", lpbuffer_st.valid_can_read);
                }

                m_bchangeH264to4k = false;
                lpbuffer_st.enlpflag = false;
                last_time = av_gettime();
                LOGI("consume time: %lld us %lld ms\n", (last_time - now_time), (last_time - now_time)/1000);
            }
        }

        if((video_status.status & DECODER_FATAL_ERROR_UNKNOW) &&
            (pcodec->video_type == VFORMAT_H264) ) {
            int app_reset_support  = amsysfs_get_sysfs_int("/sys/module/amvdec_h264/parameters/fatal_error_reset");
            if(app_reset_support){
                LOGI("fatal_error_reset=1,DECODER_FATAL_ERROR_UNKNOW happened force reset decoder\n ");
                amsysfs_set_sysfs_int("/sys/module/amvdec_h264/parameters/decoder_force_reset", 1);
            }
        }
        //monitor buffer staus ,overflow more than 2s reset player,if support
        if (prop_playerwatchdog_support && !m_bIsPause){
            codec_get_abuf_state(pcodec, &audio_buf);
            if(prop_softdemux == 0)
                codec_get_vbuf_state(pcodec, &video_buf);
            else
                codec_get_vbuf_state(vcodec, &video_buf);
            if (audio_buf.size != 0)
                audio_buf_level = (float)audio_buf.data_len / audio_buf.size;
            if (video_buf.size != 0)
                video_buf_level = (float)video_buf.data_len / video_buf.size;
            if(prev_aread != audio_buf.read_pointer)
                arp_is_changed = 1;
            else
                arp_is_changed = 0;
            if(prev_vread != video_buf.read_pointer)
                vrp_is_changed = 1;
            else
                vrp_is_changed = 0;
            prev_aread = audio_buf.read_pointer;
            prev_vread = video_buf.read_pointer;
            if (((audio_buf_level >= MAX_WRITE_ALEVEL) && !arp_is_changed) ||
                    ((video_buf_level >= MAX_WRITE_VLEVEL) && !vrp_is_changed)){
                LOGI("checkVdecstate : audio_buf_level= %.5f, video_buf_level=%.5f\n", audio_buf_level, video_buf_level);
                LOGI("prev_aread = %x,prev_vread = %x,  vrp_is_changed =%d, arp_is_changed=%d\n",
                        prev_aread, prev_vread, vrp_is_changed, arp_is_changed);
                if (m_PreviousOverflowTime == 0)
                    m_PreviousOverflowTime  = av_gettime();
                if ((av_gettime()-m_PreviousOverflowTime) >= 2000000){
                    LOGI("buffer  overflow more than 2s ,reset  player\n ");
                    iStop();
                    usleep(500*1000);
                    iStartPlay();
                }
            }else{
                m_PreviousOverflowTime = 0;
            }
        }
    }
}
void *CTsPlayer::threadCheckAbend(void *pthis) {
    LOGV("threadCheckAbend start pthis: %p\n", pthis);
    CTsPlayer *tsplayer = static_cast<CTsPlayer *>(pthis);
    do {
        usleep(50 * 1000);
        //sleep(2);
        //tsplayer->checkBuffLevel();
        if (tsplayer->m_bIsPlay) {
            checkcount++;
            if (lp_trylock(&tsplayer->mutex) != 0) {
                continue;
            }

            tsplayer->checkVdecstate();
            if(checkcount >= 40) {
                tsplayer->checkAbend();
                //tsplayer->Report_Audio_paramters();
                checkcount = 0;
            }
            lp_unlock(&tsplayer->mutex);
        }
    }
    while(!m_StopThread);
    LOGV("threadCheckAbend end\n");
    return NULL;
}

int CTsPlayer::ReportVideoFrameInfo(struct vframe_qos_s * pframe_qos)
{
    int i;
    VIDEO_FRM_STATUS_INFO_T videoFrmInfo;
    //
    if(pframe_qos[0].num == mLastVdecInfoNum)


        return 1;
        for(i=0;i<QOS_FRAME_NUM;i++) {
#if 0
                  LOGI("##kernel Info, LastNum=%d,curNum=%d, type %d size %d PTS %d QP %d MaxMV %d MinMV %d AvgMV %d AvgSkip %d\n",
                mLastVdecInfoNum,  pframe_qos[i].num,
                pframe_qos[i].type,
                pframe_qos[i].size,
                pframe_qos[i].pts,
                pframe_qos[i].avg_qp,
                pframe_qos[i].max_mv,
                pframe_qos[i].min_mv,
                pframe_qos[i].avg_mv,
                pframe_qos[i].avg_skip);
#endif
        videoFrmInfo.nVidFrmSize = pframe_qos[i].size;
        videoFrmInfo.nVidFrmPTS = pframe_qos[i].pts;
        videoFrmInfo.nVidFrmQP = pframe_qos[i].avg_qp;
        videoFrmInfo.nMaxMV = pframe_qos[i].max_mv;
        videoFrmInfo.nMinMV= pframe_qos[i].min_mv;
        videoFrmInfo.nMinMV = pframe_qos[i].avg_mv;
        videoFrmInfo.SkipRatio = pframe_qos[i].avg_skip;
		//LOGD("GetVideoFrameInfo  info_num=%d (%d-%d)  %d\n",
		//	info_num, curVdecInfoNum, tsplayer->mLastVdecInfoNum, frametype);



        videoFrmInfo.enVidFrmType = (VID_FRAME_TYPE_e) pframe_qos[i].type;
        if (pframe_qos[i].type == 4)
            videoFrmInfo.enVidFrmType = VID_FRAME_TYPE_I;
        LOGD("##Vdec Info, LastNum=%d,curNum=%d, type %d size %d PTS %d QP %d MaxMV %d MinMV %d AvgMV %d AvgSkip %d\n",
                mLastVdecInfoNum, pframe_qos[i].num,
                videoFrmInfo.enVidFrmType,
                videoFrmInfo.nVidFrmSize,
                videoFrmInfo.nVidFrmPTS,
                videoFrmInfo.nVidFrmQP,
                videoFrmInfo.nMaxMV,
                videoFrmInfo.nMinMV,
                videoFrmInfo.nAvgMV,
                videoFrmInfo.SkipRatio);

        if (pfunc_player_param_evt != NULL) {
            pfunc_player_param_evt(player_evt_param_handler, IPTV_PLAYER_PARAM_EVT_VIDFRM_STATUS_REPORT, &videoFrmInfo);
        }

    }
    mLastVdecInfoNum  = pframe_qos[0].num;
    return 0;
}

void *CTsPlayer::threadReportInfo(void *pthis) {
    LOGI("threadGetVideoInfo start pthis: %p\n", pthis);
    CTsPlayer *tsplayer = static_cast<CTsPlayer *>(pthis);
    int ret = 0;
    int max_count = 0;
    do {
            if (tsplayer->m_bIsPlay) {
                tsplayer->update_stream_bitrate();
                if (tsplayer->m_sCtsplayerState.first_picture_comming== 0) {
                    LOGI("first updateCTCInfo,width :%d,height:%d\n",
                    tsplayer->m_sCtsplayerState.video_width,
                    tsplayer->m_sCtsplayerState.video_height);
                    max_count = 1;
                }else
                     max_count = 40;
                checkcount1++;
                if(checkcount1 >= max_count) {
                    //tsplayer->Report_video_paramters();
                    tsplayer->updateCTCInfo();
                    checkcount1 = 0;
                }
            }
            usleep(50 * 1000);
    }while(!m_StopThread);
    LOGI("threadGetVideoInfo end\n");
    return NULL;
}

void *CTsPlayer::threadReadPacket(void *pthis) {
    LOGV("threadReadPacket start pthis: %p\n", pthis);
    CTsPlayer *tsplayer = static_cast<CTsPlayer *>(pthis);
    do {
        usleep(prop_readffmpeg*1000);
        if (tsplayer->m_bIsPlay)
            tsplayer->readExtractor();
    }
    while(!m_StopThread);
    LOGV("threadReadPakcet end\n");
    return NULL;
}







int CTsPlayer::GetRealTimeFrameRate()
{
    if (m_sCtsplayerState.current_fps > 0)

        LOGI( "realtime fps:%d\n", m_sCtsplayerState.current_fps);
    return m_sCtsplayerState.current_fps;
}

int CTsPlayer::GetVideoFrameRate()
{


    return m_sCtsplayerState.frame_rate;
}
int CTsPlayer::GetVideoDropNumber()
{
	int drop_number = 0;
	drop_number = m_sCtsplayerState.vdec_drop;
	LOGI("video drop number = %d\n",drop_number);

	return drop_number;
}
int CTsPlayer::GetVideoTotalNumber()
{
	int total_number = 0;
	total_number = m_sCtsplayerState.vdec_total;
	LOGI("video total number = %d\n",total_number);

	return total_number;
}

int CTsPlayer::updateCTCInfo()
{
    struct av_param_info_t av_param_info;
    memset(&av_param_info , 0 ,sizeof(av_param_info));
    if (pcodec->has_video) {
        if(prop_softdemux == 0) {
            codec_get_av_param_info(pcodec, &av_param_info);
        }else{
            codec_get_av_param_info(vcodec, &av_param_info);
        }
    }
    // check first frame comming
    if (av_param_info.av_info.first_pic_coming) {
        m_sCtsplayerState.first_picture_comming = 1;
        } else {
            return 0;
        }

    ReportVideoFrameInfo(av_param_info.vframe_qos);
    update_caton_info(&av_param_info);
#ifdef DEBUG_INFO
    LOGI("apts:%x,aptserr:%d,vpts:%x,vptserr:%d,toggle:%d,curfps:%d,dec(%d,%d,%d),ts_err:%d,fvpts:%x,format:%d,(%dx%d)\n",
        av_param_info.av_info.apts,
        av_param_info.av_info.apts_err,
        av_param_info.av_info.vpts,
        av_param_info.av_info.vpts_err,
        av_param_info.av_info.toggle_frame_count,
        av_param_info.av_info.current_fps,
        av_param_info.av_info.dec_drop_frame_count,
        av_param_info.av_info.dec_err_frame_count,
        av_param_info.av_info.dec_frame_count,
        av_param_info.av_info.ts_error,
        av_param_info.av_info.first_vpts,
        av_param_info.av_info.frame_format,
        av_param_info.av_info.width,
        av_param_info.av_info.height
    );
#endif
    buf_status audio_buf;
    buf_status video_buf;
    int64_t  last_time, now_time;

    if (pcodec->has_audio) {
        unsigned long audiopts;
        codec_get_abuf_state(pcodec, &audio_buf);
        codec_get_audio_basic_info(pcodec);
        codec_get_audio_cur_bitrate(pcodec, &pcodec->audio_info.bitrate);

        m_sCtsplayerState.apts = (int64_t)av_param_info.av_info.apts;
        m_sCtsplayerState.samplate = pcodec->audio_info.sample_rate;
        m_sCtsplayerState.channel= pcodec->audio_info.channels;
        m_sCtsplayerState.bitrate= pcodec->audio_info.bitrate;
        m_sCtsplayerState.abuf_used = audio_buf.data_len;
        m_sCtsplayerState.abuf_size = audio_buf.size;
	if (pcodec->audio_info.error_num != m_sCtsplayerState.adec_error) {
            pfunc_player_evt(IPTV_PLAYER_EVT_AUD_FRAME_ERROR, player_evt_hander);
            pfunc_player_evt(IPTV_PLAYER_EVT_AUD_DISCARD_FRAME, player_evt_hander);
	}
	if (adec_underflow != m_sCtsplayerState.adec_underflow) {
            pfunc_player_evt(IPTV_PLAYER_EVT_AUD_DEC_UNDERFLOW, player_evt_hander);
	}
        if (av_param_info.av_info.apts_err != m_sCtsplayerState.apts_error) {
            pfunc_player_evt(IPTV_PLAYER_EVT_AUD_PTS_ERROR, player_evt_hander);
	}
        m_sCtsplayerState.adec_error = pcodec->audio_info.error_num;
        m_sCtsplayerState.adec_drop = pcodec->audio_info.error_num;
        m_sCtsplayerState.adec_underflow = adec_underflow;
        m_sCtsplayerState.apts_error = av_param_info.av_info.apts_err;
        LOGV("audio: pts:0x%llx, samplate=%d, channel=%d, bitrate=%d\n",m_sCtsplayerState.apts,
            m_sCtsplayerState.samplate,m_sCtsplayerState.channel,m_sCtsplayerState.bitrate);
        LOGV("audio-2: abuf_used=%d, abuf_size=%dKB, adec_error=%d, adec_underflow=%d, apts_error=%d\n",m_sCtsplayerState.abuf_used,
            m_sCtsplayerState.abuf_size/1024,m_sCtsplayerState.adec_error,m_sCtsplayerState.adec_underflow,m_sCtsplayerState.apts_error);
    }
    if (pcodec->has_video) {
        unsigned long videopts;
        if(prop_softdemux == 0) {
            codec_get_vbuf_state(pcodec, &video_buf);
        }else{
            codec_get_vbuf_state(vcodec, &video_buf);
        }

        m_sCtsplayerState.vpts = (int64_t)av_param_info.av_info.vpts_err;
        m_sCtsplayerState.video_width = av_param_info.av_info.width;
        m_sCtsplayerState.video_height = av_param_info.av_info.height;
        m_sCtsplayerState.frame_rate = av_param_info.av_info.fps;
        m_sCtsplayerState.current_fps = av_param_info.av_info.current_fps;
        if (( m_sCtsplayerState.video_width==640) &&
            (m_sCtsplayerState.video_height==480)) {
            m_sCtsplayerState.video_ratio = 0;
        } else if (( m_sCtsplayerState.video_width==720) &&
            (m_sCtsplayerState.video_height==576)) {
            m_sCtsplayerState.video_ratio = 1;
        } else if (( m_sCtsplayerState.video_width==1280) &&
            (m_sCtsplayerState.video_height==720)) {
            m_sCtsplayerState.video_ratio = 2;
        } else if (( m_sCtsplayerState.video_width==1920) &&
            (m_sCtsplayerState.video_height==1080)) {
            m_sCtsplayerState.video_ratio = 3;
        } else if (( m_sCtsplayerState.video_width==3840) &&
            (m_sCtsplayerState.video_height==2160)) {
            m_sCtsplayerState.video_ratio = 4;
        } else {
            m_sCtsplayerState.video_ratio = 5;
        }
        float videoWH = 0;
        if(m_sCtsplayerState.video_width  > 0 && m_sCtsplayerState.video_width  > 0)
            videoWH =(float)m_sCtsplayerState.video_width  / (float)m_sCtsplayerState.video_width ;
        if (videoWH < 1.34) {
            m_sCtsplayerState.video_rWH = 0;
        } else if(videoWH > 1.7) {
            m_sCtsplayerState.video_rWH = 1;
        }
        m_sCtsplayerState.Video_frame_format = av_param_info.av_info.frame_format == FRAME_FORMAT_PROGRESS ? 1:0;
        m_sCtsplayerState.vbuf_used = video_buf.data_len;
        m_sCtsplayerState.vbuf_size = video_buf.size;
	if (m_sCtsplayerState.vdec_error != av_param_info.av_info.dec_error_count) {
	    pfunc_player_evt(IPTV_PLAYER_EVT_VID_FRAME_ERROR, player_evt_hander); // use video underflow
	}
	if (m_sCtsplayerState.vdec_drop != av_param_info.av_info.dec_drop_frame_count) {
            pfunc_player_evt(IPTV_PLAYER_EVT_VID_DISCARD_FRAME, player_evt_hander);
	}

	if (m_sCtsplayerState.vdec_underflow != vdec_underflow) {
            pfunc_player_evt(IPTV_PLAYER_EVT_VID_DEC_UNDERFLOW, player_evt_hander);
	}

	if (m_sCtsplayerState.vpts_error != av_param_info.av_info.ts_error) {
	    pfunc_player_evt(IPTV_PLAYER_EVT_VID_PTS_ERROR, player_evt_hander);
	}
        m_sCtsplayerState.vdec_total = av_param_info.av_info.dec_frame_count;;
        m_sCtsplayerState.vdec_error = av_param_info.av_info.dec_error_count;
        m_sCtsplayerState.vdec_drop = av_param_info.av_info.dec_drop_frame_count;
        m_sCtsplayerState.vdec_underflow = vdec_underflow;
        m_sCtsplayerState.vpts_error = av_param_info.av_info.vpts_err;
	LOGV("video: vpts=0x%llx, width=%d, height=%d, ratio=%d, rWh=%d, frame_format=%d\n",m_sCtsplayerState.vpts,m_sCtsplayerState.video_width,
            m_sCtsplayerState.video_height,m_sCtsplayerState.video_ratio,m_sCtsplayerState.video_rWH,m_sCtsplayerState.Video_frame_format);
        LOGV("video-2:  vbuf_used=%d, vbuf_size=%dKB, vdec_error=%d, vdec_underflow=%d, vpts_err=%d\n",
		m_sCtsplayerState.vbuf_used,m_sCtsplayerState.vbuf_size/1024,m_sCtsplayerState.vdec_error,m_sCtsplayerState.vdec_underflow,m_sCtsplayerState.vpts_error);
    }

    m_sCtsplayerState.avdiff = llabs(m_sCtsplayerState.apts - m_sCtsplayerState.vpts);
    m_sCtsplayerState.ff_mode = (m_bFast == true);
    m_sCtsplayerState.ts_error = av_param_info.av_info.ts_error;
    m_sCtsplayerState.ts_sync_loss = 0; // unkown
    m_sCtsplayerState.ecm_error = 0; // unkown

    m_sCtsplayerState.valid = 1;

    LOGV("player: avdiff=%lld,ff_mode=%d,ts_error=%d\n",m_sCtsplayerState.avdiff,m_sCtsplayerState.ff_mode,m_sCtsplayerState.ts_error);
    if (m_sCtsplayerState.first_picture_comming == 1) {
        m_sCtsplayerState.first_frame_pts = av_param_info.av_info.first_vpts;
        pfunc_player_evt(IPTV_PLAYER_EVT_FIRST_PTS, player_evt_hander);
    }

    return 0;

}


int CTsPlayer::playerback_getStatusInfo(IPTV_ATTR_TYPE_e enAttrType, int *value)
{
    if (value == NULL) {
        LOGI("[%s],point of value is NULL\n",__func__);
        return 0;
    }
	switch(enAttrType) {
	case IPTV_PLAYER_ATTR_VID_ASPECT :
		*value = m_sCtsplayerState.video_ratio;
	    break;
	case IPTV_PLAYER_ATTR_VID_RATIO :
		*value = m_sCtsplayerState.video_rWH;
		break;
	case IPTV_PLAYER_ATTR_VID_SAMPLETYPE :
		*value = m_sCtsplayerState.Video_frame_format;
		break;
	case IPTV_PLAYER_ATTR_VIDAUDDIFF :
		*value = m_sCtsplayerState.avdiff;
		break;
	case IPTV_PLAYER_ATTR_VID_BUF_SIZE :
		*value = m_sCtsplayerState.vbuf_size;
		break;
	case IPTV_PLAYER_ATTR_VID_USED_SIZE :
		*value = m_sCtsplayerState.vbuf_used;
		break;
	case IPTV_PLAYER_ATTR_AUD_BUF_SIZE :
		*value = m_sCtsplayerState.abuf_size;
		break;
	case IPTV_PLAYER_ATTR_AUD_USED_SIZE :
		*value = m_sCtsplayerState.abuf_used;
		break;
	case IPTV_PLAYER_ATTR_AUD_SAMPLERATE:
		*value = m_sCtsplayerState.samplate;
		break;
	case IPTV_PLAYER_ATTR_AUD_BITRATE:
		*value = m_sCtsplayerState.bitrate;
		break;
	case IPTV_PLAYER_ATTR_AUD_CHANNEL_NUM:
		*value = m_sCtsplayerState.channel;
		break;
	case IPTV_PLAYER_ATTR_BUTT :
		LOGV("--IPTV_PLAYER_ATTR_BUTT\n");
		break;
	case IPTV_PLAYER_ATTR_V_FRAME_RATE:
		GetVideoFrameRate();
		*value = m_sCtsplayerState.frame_rate;
		break;
	case IPTV_PLAYER_ATTR_V_HEIGHT:
		*value = m_sCtsplayerState.video_height;
		break;
	case IPTV_PLAYER_ATTR_V_WIDTH:
		*value = m_sCtsplayerState.video_width;
		break;
	case IPTV_PLAYER_ATTR_STREAM_BITRATE:
		*value = m_sCtsplayerState.stream_bitrate;
		break;
	case IPTV_PLAYER_ATTR_CATON_TIMES:
		*value = m_sCtsplayerState.caton_times;
		break;
	case IPTV_PLAYER_ATTR_CATON_TIME:
		*value = m_sCtsplayerState.caton_time;
		break;
	default:
		LOGV("unknow type");
		break;

	}
	LOGI("---playerback_getStatusInfo, enAttrType=%d,value=%d\n",enAttrType,*value);

	return 0;

}
void CTsPlayer::SwitchAudioTrack_ZTE(PAUDIO_PARA_T pAudioPara) {}
void CTsPlayer::SetVideoHole(int x,int y,int w,int h) {}
void CTsPlayer::writeScaleValue() {}
int CTsPlayer::GetCurrentVidPTS(unsigned long long *pPTS) {return 0;}
void CTsPlayer::GetVideoInfo(int *width, int *height, int *ratio) {}
int CTsPlayer::GetPlayerInstanceNo() {return 0;}
void CTsPlayer::ExecuteCmd(const char* cmd_str) {}
bool CTsPlayer::StartRender() {return true;}

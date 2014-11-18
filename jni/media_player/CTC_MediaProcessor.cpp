/**
 * @file 		iptv_player_jni.cpp
 * @author    	zhouyj
 * @date      	2012/9/5
 * @version   	ver 1.0
 * @brief     	定义CTC_MediaProcessor类中方法的jni接口，供上层调用。
 * @attention
*/
#include "player.h"
#include "player_type.h"
#include "vformat.h"
#include "aformat.h"
#include "android_runtime/AndroidRuntime.h"
#include <gui/Surface.h> 
#include <gui/ISurfaceTexture.h>
#include "android_runtime/android_view_Surface.h"
#include "Proxy_MediaProcessor.h"
using namespace android;


#include "android/log.h"
#include "jni.h"
#include "stdio.h"
#include <android/bitmap.h>
#include <string.h>

#define  LOG_TAG    "CTC_MediaControl"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define BUFF_SIZE (32*1024)

Mutex    gMutexLock;

static FILE * g_TestResultLogHandle = NULL;

static FILE * GetTestResultLogHandle()
{
	  if(g_TestResultLogHandle == NULL)
	  {
	    g_TestResultLogHandle = fopen("/data/data/com.ctc/mediaTestSuite.txt", "wb");
	    if(g_TestResultLogHandle == NULL)
		  {
		    LOGE("create file :error");
		    return NULL;
		  }
		  else
		  {
		  		char  writebuf[2000];
	        unsigned int buflen = 2000;
	        memset(writebuf,0,buflen);
	        snprintf(writebuf,buflen,"%s\r\n%s\r\n","ctc_player test result:","*********************"); 
	        writebuf[buflen -1] = 0;
	        fwrite(writebuf,1,strlen(writebuf),g_TestResultLogHandle);
	        fflush(g_TestResultLogHandle);
		  }
	   }
	

	return g_TestResultLogHandle;
	
}

#ifdef __cplusplus
extern "C" {
#endif

Proxy_MediaProcessor* proxy_mediaProcessor = NULL; 
FILE *fp;
int isPause = 0;

//将test结果输出到文件
void Java_com_ctc_MediaProcessorDemoActivity_nativeWriteFile(JNIEnv* env, jobject thiz, jstring Function, jstring Return, jstring Result)
{
	FILE *result_fp = GetTestResultLogHandle();
	
	if(result_fp == NULL)
	{
		LOGE("create file :error");
		return;
	}
	const char* Function_t = (*env).GetStringUTFChars(Function, NULL);
	const char* Return_t = (*env).GetStringUTFChars(Return, NULL);
	const char* Result_t = (*env).GetStringUTFChars(Result, NULL);
	char *divide_str = "*********************";
	char  writebuf[2000];
	unsigned int buflen = 2000;
	memset(writebuf,0,buflen);
	snprintf(writebuf,buflen,"%s\r\n%s\r\n%s\r\n%s\r\n",Function_t, Return_t, Result_t, divide_str);
	writebuf[buflen -1] = 0;
	fwrite(writebuf,1,strlen(writebuf),result_fp);
	fflush(result_fp);
	return;
}

//从java层获取surface
jint Java_com_ctc_MediaProcessorDemoActivity_nativeCreateSurface(JNIEnv* env, jobject thiz, jobject pSurface, int w, int h)
{
	
	LOGI("get the surface");
	//sp<Surface> surface(Surface_getSurface(env, pSurface));	//This is for ICS
	sp<Surface> surface(android_view_Surface_getSurface(env, pSurface));
	
//mySurface = getNativeSurface(env, pSurface);
//ctc_MediaControl->SetSurface(mySurface);
	LOGI("success: get surface");
	proxy_mediaProcessor->Proxy_SetSurface(surface.get());
	LOGI("success: set surface ");
	proxy_mediaProcessor->Proxy_SetEPGSize(w, h);
	
		
	return 0;
} 

//设置EPG大小
void Java_com_ctc_MediaProcessorDemoActivity_nativeSetEPGSize(JNIEnv* env, jobject thiz, int w, int h)
{
	proxy_mediaProcessor->Proxy_SetEPGSize(w, h);
	return;
}


static void signal_handler(int signum)
{   
	ALOGI("Get signum=%x",signum);
	player_progress_exit();	
	signal(signum, SIG_DFL);
	raise (signum);
}

int _media_info_dump(media_info_t* minfo)
{
    int i = 0;
    ALOGI("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    ALOGI("======||file size:%lld\n",minfo->stream_info.file_size);
    ALOGI("======||file type:%d\n",minfo->stream_info.type); 
    ALOGI("======||has internal subtitle?:%s\n",minfo->stream_info.has_sub>0?"YES!":"NO!");
    ALOGI("======||internal subtile counts:%d\n",minfo->stream_info.total_sub_num);
    ALOGI("======||has video track?:%s\n",minfo->stream_info.has_video>0?"YES!":"NO!");
    ALOGI("======||has audio track?:%s\n",minfo->stream_info.has_audio>0?"YES!":"NO!");    
    ALOGI("======||duration:%d\n",minfo->stream_info.duration);
    if(minfo->stream_info.has_video &&minfo->stream_info.total_video_num>0)
    {        
        ALOGI("======||video counts:%d\n",minfo->stream_info.total_video_num);
        ALOGI("======||video width:%d\n",minfo->video_info[0]->width);
        ALOGI("======||video height:%d\n",minfo->video_info[0]->height);
        ALOGI("======||video bitrate:%d\n",minfo->video_info[0]->bit_rate);
        ALOGI("======||video format:%d\n",minfo->video_info[0]->format);

    }
    if(minfo->stream_info.has_audio && minfo->stream_info.total_audio_num> 0)
    {
        ALOGI("======||audio counts:%d\n",minfo->stream_info.total_audio_num);
        
        if(NULL !=minfo->audio_info[0]->audio_tag)
        {
            ALOGI("======||track title:%s",minfo->audio_info[0]->audio_tag->title!=NULL?minfo->audio_info[0]->audio_tag->title:"unknow");   
            ALOGI("\n======||track album:%s",minfo->audio_info[0]->audio_tag->album!=NULL?minfo->audio_info[0]->audio_tag->album:"unknow"); 
            ALOGI("\n======||track author:%s\n",minfo->audio_info[0]->audio_tag->author!=NULL?minfo->audio_info[0]->audio_tag->author:"unknow");
            ALOGI("\n======||track year:%s\n",minfo->audio_info[0]->audio_tag->year!=NULL?minfo->audio_info[0]->audio_tag->year:"unknow");
            ALOGI("\n======||track comment:%s\n",minfo->audio_info[0]->audio_tag->comment!=NULL?minfo->audio_info[0]->audio_tag->comment:"unknow"); 
            ALOGI("\n======||track genre:%s\n",minfo->audio_info[0]->audio_tag->genre!=NULL?minfo->audio_info[0]->audio_tag->genre:"unknow");
            ALOGI("\n======||track copyright:%s\n",minfo->audio_info[0]->audio_tag->copyright!=NULL?minfo->audio_info[0]->audio_tag->copyright:"unknow");  
            ALOGI("\n======||track track:%d\n",minfo->audio_info[0]->audio_tag->track);  
        }
            

        
        for(i = 0;i<minfo->stream_info.total_audio_num;i++)
        {
            ALOGI("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
            ALOGI("======||%d 'st audio track codec type:%d\n",i,minfo->audio_info[i]->aformat);
            ALOGI("======||%d 'st audio track audio_channel:%d\n",i,minfo->audio_info[i]->channel);
            ALOGI("======||%d 'st audio track bit_rate:%d\n",i,minfo->audio_info[i]->bit_rate);
            ALOGI("======||%d 'st audio track audio_samplerate:%d\n",i,minfo->audio_info[i]->sample_rate);
            ALOGI("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
            
        }
        
    }
    if(minfo->stream_info.has_sub &&minfo->stream_info.total_sub_num>0){
        for(i = 0;i<minfo->stream_info.total_sub_num;i++)
        {
            if(0 == minfo->sub_info[i]->internal_external){
                ALOGI("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                ALOGI("======||%d 'st internal subtitle pid:%d\n",i,minfo->sub_info[i]->id);   
                ALOGI("======||%d 'st internal subtitle language:%s\n",i,minfo->sub_info[i]->sub_language?minfo->sub_info[i]->sub_language:"unknow"); 
                ALOGI("======||%d 'st internal subtitle width:%d\n",i,minfo->sub_info[i]->width); 
                ALOGI("======||%d 'st internal subtitle height:%d\n",i,minfo->sub_info[i]->height); 
                ALOGI("======||%d 'st internal subtitle resolution:%d\n",i,minfo->sub_info[i]->resolution); 
                ALOGI("======||%d 'st internal subtitle subtitle size:%lld\n",i,minfo->sub_info[i]->subtitle_size); 
                ALOGI("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");       
            }
        }
    }
    ALOGI("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    return 0;
}
//根据音视频参数初始化播放器
jint Java_com_ctc_MediaProcessorDemoActivity_nativeInit(JNIEnv* env, jobject thiz, jstring url)
{
	VIDEO_PARA_T  videoPara={0};
	AUDIO_PARA_T audioPara[MAX_AUDIO_PARAM_SIZE]={0};
	SUBTITLE_PARA_T sParam[MAX_SUBTITLE_PARAM_SIZE]={0};
	//get video pids  merge form kplayer
	play_control_t *pCtrl = NULL; 
	int pid;
	media_info_t minfo; 
	const char* URL = (*env).GetStringUTFChars(url, NULL);
	pCtrl = (play_control_t*)malloc(sizeof(play_control_t));  
	memset(pCtrl,0,sizeof(play_control_t));   
	memset(&minfo,0,sizeof(media_info_t));
	player_init();
	pCtrl->file_name=strdup(URL);
	pCtrl->video_index = -1;// MUST
	pCtrl->audio_index = -1;// MUST
	pCtrl->sub_index = -1;/// MUST 
	pCtrl->hassub = 1;  // enable subtitle
	pCtrl->t_pos = -1;  // start position, if live streaming, need set to -1
	pCtrl->need_start = 1; // if 0,you can omit player_start_play API.just play video/audio immediately. if 1,need call "player_start_play" API;
	pid=player_start(pCtrl,0);
	if(pid<0)
	{
	  ALOGI("player start failed!error=%d",pid);
	  goto fail;
	}
	signal(SIGSEGV, signal_handler);
	while(!PLAYER_THREAD_IS_STOPPED(player_get_state(pid))){
	  if(player_get_state(pid) >= PLAYER_INITOK) {
	    int ret = player_get_media_info(pid,&minfo);
	    int i;
	    if(ret==0){
	      ALOGI("player_get_media_info success pid=%d ",pid);
	      _media_info_dump(&minfo); 
	      if(minfo.stream_info.has_video &&minfo.stream_info.total_video_num>0){
	        videoPara.pid=minfo.video_info[0]->id;
	        videoPara.vFmt=minfo.video_info[0]->format;
	        ALOGI("player_get_media_info get video pid  %d  ",videoPara.pid);
	        if(videoPara.vFmt!=VFORMAT_H264){
	        ALOGI("player_get_media_info failed vFmt %d !=VFORMAT_H264 ",videoPara.vFmt);
	        goto fail;
	        }
	      }
	      if(minfo.stream_info.has_audio && minfo.stream_info.total_audio_num> 0){
	        for(i = 0;i<minfo.stream_info.total_audio_num;i++){
	          audioPara[i].pid=minfo.audio_info[i]->id; 
	          ALOGI("player_get_media_info get audio pid  %d  ",audioPara[i].pid);                
	        }
	      }       
	      if(minfo.stream_info.has_sub &&minfo.stream_info.total_sub_num>0){
	      for(i = 0;i<minfo.stream_info.total_sub_num;i++){
	        if(0 == minfo.sub_info[i]->internal_external){          
	        sParam[i].pid=minfo.sub_info[i]->id;   
	        sParam[i].sub_type=minfo.sub_info[i]->sub_type;
	        ALOGI("player_get_media_info get subtitle  pid  %d  sub_type %d",sParam[i].pid,sParam[i].sub_type); 
	        }
	      }
	    }
	    break;
	  }
	  else{
	    ALOGI("player_get_media_info failed pid=%d ",pid);
	    goto fail;
	  }       
	}
	usleep(100*1000);
	signal(SIGCHLD, SIG_IGN);        
	signal(SIGTSTP, SIG_IGN);        
	signal(SIGTTOU, SIG_IGN);        
	signal(SIGTTIN, SIG_IGN);        
	signal(SIGHUP, signal_handler);        
	signal(SIGTERM, signal_handler);        
	signal(SIGSEGV, signal_handler);        
	signal(SIGINT, signal_handler);        
	signal(SIGQUIT, signal_handler);
	}
	player_stop(pid);
	player_exit(pid);
	free(pCtrl->file_name);
	free(pCtrl);

	proxy_mediaProcessor=new Proxy_MediaProcessor();
	
/*	videoPara.pid = 0x45;//101;
	videoPara.nVideoWidth = 544;
	videoPara.nVideoHeight = 576;
	videoPara.nFrameRate = 25;
	videoPara.vFmt = VFORMAT_H264;
	videoPara.cFmt = 0;
	videoPara.vFmt=VFORMAT_H264;*/
	
	
/*	audioPara.pid = 0x44;//144;
	audioPara.nChannels = 1;
	audioPara.nSampleRate = 48000;
	audioPara.aFmt = AFORMAT_MPEG;
	audioPara.nExtraSize = 0;
	audioPara.pExtraData = NULL;*/
	
	proxy_mediaProcessor->Proxy_InitVideo(&videoPara);
	proxy_mediaProcessor->Proxy_InitAudio(audioPara);
	
/*	
	sParam[0].pid=0x106;
	sParam[0].sub_type=CODEC_ID_DVB_SUBTITLE;
	sParam[1].pid=0x107;
	sParam[1].sub_type=CODEC_ID_DVB_SUBTITLE;
	sParam[2].pid=0x108;
	sParam[2].sub_type=CODEC_ID_DVB_SUBTITLE;
	sParam[3].pid=0x109;
	sParam[3].sub_type=CODEC_ID_DVB_SUBTITLE;*/
	
	proxy_mediaProcessor->Proxy_InitSubtitle(sParam);
	return 0;
	
fail:
	if(pid>0)
		player_exit(pid);
	free(pCtrl->file_name);
	free(pCtrl);
	return -1;
}

//开始播放
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeStartPlay(JNIEnv* env, jobject thiz)
{
	jboolean result = proxy_mediaProcessor->Proxy_StartPlay();
	return result;
}

//写入数据播放
jint Java_com_ctc_MediaProcessorDemoActivity_nativeWriteData(JNIEnv* env, jobject thiz, jstring url)
{
	const char* URL = (*env).GetStringUTFChars(url, NULL);
	
	int rd_result = 0;
	fp = fopen(URL, "rb+");
	if (fp == NULL)
		{
		LOGE("open file:error!");
		return -1;
	}
	
	while(true)
	{
	 {
	 	Mutex::Autolock l(gMutexLock);
		char* buffer = (char* )malloc(BUFF_SIZE);
		rd_result = fread(buffer, BUFF_SIZE, 1, fp);
		if (rd_result <= 0)	
			{
				LOGE("read the end of file");
			//	proxy_mediaProcessor->~Proxy_MediaProcessor();
				exit(1);
			}
		
		int wd_result = proxy_mediaProcessor->Proxy_WriteData((unsigned char*) buffer, (unsigned int) BUFF_SIZE);
		LOGE("the wd_result[%d]", wd_result);
		
	 }
	 
	 usleep(130*1000);
	
	 
	}
	return 0;
	
}

 
//取得播放模式
jint Java_com_ctc_MediaProcessorDemoActivity_nativeGetPlayMode(JNIEnv* env, jobject thiz)
{
	int result = proxy_mediaProcessor->Proxy_GetPlayMode();
	LOGE("step:1");

  return (jint)result;
}

//设置播放区域的位置和播放区域的宽高
jint Java_com_ctc_MediaProcessorDemoActivity_nativeSetVideoWindow(JNIEnv* env, jobject thiz ,jint x, jint y, jint width, jint height)
{
	int result = proxy_mediaProcessor->Proxy_SetVideoWindow(x, y, width, height);
	LOGE("SetVideoWindow result:[%d]", result);
	return result;
}

//播放器暂停
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativePause(JNIEnv* env, jobject thiz)
{
	LOGE("NEXT:Pause");
	isPause = 1;
	jboolean result = proxy_mediaProcessor->Proxy_Pause();
	return result;
}
//播放器继续播放
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeResume(JNIEnv* env, jobject thiz)
{
	LOGE("NEXT:Resume");
	isPause = 0;
	jboolean result = proxy_mediaProcessor->Proxy_Resume();
	return result;
}

//播放器选时
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeSeek(JNIEnv* env, jobject thiz)
{
	LOGE("nativeSeek---0");
	Mutex::Autolock l(gMutexLock);
	LOGE("nativeSeek---1");	
	fseek(fp, 0, 0);
	LOGE("nativeSeek---2");
	jboolean result = proxy_mediaProcessor->Proxy_Seek();
	LOGE("nativeSeek---3");
	return result;
}

//显示视频
jint Java_com_ctc_MediaProcessorDemoActivity_nativeVideoShow(JNIEnv* env, jobject thiz)
{
	jint result = proxy_mediaProcessor->Proxy_VideoShow();
	return result;
}

//隐藏视频
jint Java_com_ctc_MediaProcessorDemoActivity_nativeVideoHide(JNIEnv* env, jobject thiz)
{
	jint result = proxy_mediaProcessor->Proxy_VideoHide();
	return result;
}

//快进快退
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeFast(JNIEnv* env, jobject thiz)
{
	jboolean result = proxy_mediaProcessor->Proxy_Fast();
	return result;
}

//停止快进快退
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeStopFast(JNIEnv* env, jobject thiz)
{
	jboolean result = proxy_mediaProcessor->Proxy_StopFast();
	return result;
}

//停止播放
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeStop(JNIEnv* env, jobject thiz)
{
	jboolean result = proxy_mediaProcessor->Proxy_Stop();
	
	return result;
}

//获取音量
jint Java_com_ctc_MediaProcessorDemoActivity_nativeGetVolume(JNIEnv* env, jobject thiz)
{
	jint result = proxy_mediaProcessor->Proxy_GetVolume();
	LOGE("the volume is [%d]",result);
	return result;
}

//设定音量
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeSetVolume(JNIEnv* env, jobject thiz,jint volume)
{
	jboolean result = proxy_mediaProcessor->Proxy_SetVolume(volume);
	return result;
}

//设定视频显示比例
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeSetRatio(JNIEnv* env, jobject thiz,jint nRatio)
{
	jboolean result = proxy_mediaProcessor->Proxy_SetRatio(nRatio);
	return result;
}

//获取当前声道
jint Java_com_ctc_MediaProcessorDemoActivity_nativeGetAudioBalance(JNIEnv* env, jobject thiz)
{
	jint result = proxy_mediaProcessor->Proxy_GetAudioBalance();
	return result;
}

//设置声道
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeSetAudioBalance(JNIEnv* env, jobject thiz, jint nAudioBalance)
{
	jboolean result = proxy_mediaProcessor->Proxy_SetAudioBalance(nAudioBalance);
	return result;
}

//获取视频分辨率
void Java_com_ctc_MediaProcessorDemoActivity_nativeGetVideoPixels(JNIEnv* env, jobject thiz)
{
	int width;
	int height;
	LOGE("the video prixels ");
	proxy_mediaProcessor->Proxy_GetVideoPixels(width, height);
	LOGE("the video prixels :[%d]*[%d]",width, height);
	return;
}

//获取是否由软件拉伸，如果由硬件拉伸，返回false
jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeIsSoftFit(JNIEnv* env, jobject thiz)
{
	jboolean result = proxy_mediaProcessor->Proxy_IsSoftFit();
	return result;
}

jint Java_com_ctc_MediaProcessorDemoActivity_nativeGetCurrentPlayTime(JNIEnv* env, jobject thiz)
{
	jint result = proxy_mediaProcessor->Proxy_GetCurrentPlayTime();
	return result;
}

jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeInitSubtitle(JNIEnv* env, jobject thiz)
{
	//jboolean result = proxy_mediaProcessor->Proxy_InitSubtitle();
	return true;//result;
}

jboolean Java_com_ctc_MediaProcessorDemoActivity_nativeSwitchSubtitle(JNIEnv* env, jobject thiz, jint sub_pid)
{
	proxy_mediaProcessor->Proxy_SwitchSubtitle(sub_pid);
	return true;
}

#ifdef __cplusplus
}
#endif

package dev.endfield.modloader;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import java.io.*;

public final class OfflineHostService extends Service {
    private volatile boolean stopped;
    private final java.util.concurrent.atomic.AtomicBoolean started=new java.util.concurrent.atomic.AtomicBoolean();
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public int onStartCommand(Intent intent,int flags,int id){
        if(!started.compareAndSet(false,true))return START_NOT_STICKY;
        new Thread(()->{try{
            Thread.currentThread().setContextClassLoader(NativeBridge.class.getClassLoader());
            System.loadLibrary("emodloader_host");
            String initial=ModuleStore.index(this).toString();
            RuntimeMaterializer.Source source=name->new FileInputStream(new File(new File(ModuleStore.root(this),"archives"),name.substring(4)));
            String path=RuntimeMaterializer.prepare(this,initial,source,text->android.util.Log.i("EModLoader",text));
            if(path.isEmpty()||!NativeBridge.start(path,"offline"))throw new IOException("离线宿主未能启动。");
            String applied=initial;
            while(!stopped){Thread.sleep(1000);String next=ModuleStore.index(this).toString();if(!next.equals(applied)){
                String updated=RuntimeMaterializer.prepare(this,next,source,text->android.util.Log.i("EModLoader",text));
                if(NativeBridge.update(read(updated)))applied=next;
            }}
        }catch(Exception|LinkageError error){android.util.Log.e("EModLoader","Offline host failed",error);stopSelf();}},"EML-Offline").start();
        return START_NOT_STICKY;
    }
    static String read(String path)throws IOException{try(InputStream input=new FileInputStream(path);ByteArrayOutputStream bytes=new ByteArrayOutputStream()){byte[] buffer=new byte[4096];int n;while((n=input.read(buffer))!=-1){if(bytes.size()+n>1024*1024)throw new IOException("Index too large");bytes.write(buffer,0,n);}return bytes.toString(java.nio.charset.StandardCharsets.UTF_8.name());}}
    @Override public void onDestroy(){stopped=true;try{NativeBridge.stop();}catch(LinkageError ignored){}super.onDestroy();android.os.Process.killProcess(android.os.Process.myPid());}
}

package dev.endfield.modloader;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;
import io.github.libxposed.service.XposedService;
import io.github.libxposed.service.XposedServiceHelper;
import java.io.*;
import java.util.Map;

final class FrameworkSettings {
    private static volatile XposedService service;
    private static SharedPreferences local;
    private static final SharedPreferences.OnSharedPreferenceChangeListener listener=(prefs,key)->publish();
    static SharedPreferences open(Context context){return context.getSharedPreferences("module_settings",0);}
    static void initialize(Context context){
        local=open(context);local.registerOnSharedPreferenceChangeListener(listener);
        XposedServiceHelper.registerListener(new XposedServiceHelper.OnServiceListener(){
            @Override public void onServiceBind(XposedService connected){service=connected;new Thread(()->{
                try{File directory=new File(ModuleStore.root(context),"archives");File[] archives=directory.listFiles();if(archives!=null)for(File file:archives)if(file.getName().matches("[a-f0-9-]{36}\\.zip"))publishThirdParty(file,"tpm-"+file.getName());publish();}
                catch(Exception error){android.util.Log.e("EModLoader","Archive publication failed",error);}
            },"EML-Publish").start();}
            @Override public void onServiceDied(XposedService disconnected){if(service==disconnected)service=null;}
        });
    }
    // Local package management and the offline Host do not require a framework connection.
    static void awaitThirdPartyConnection(){}
    static synchronized void publishThirdParty(File file,String name)throws IOException{
        XposedService current=service;if(current==null)return;
        if(!name.matches("tpm-[a-f0-9-]{36}\\.zip")||!file.isFile())throw new IOException("无效的模块 ZIP。");
        try(ParcelFileDescriptor descriptor=current.openRemoteFile(name);InputStream input=new FileInputStream(file);FileOutputStream output=new FileOutputStream(descriptor.getFileDescriptor())){
            output.getChannel().truncate(0);byte[] buffer=new byte[65536];int count;long size=0;
            while((count=input.read(buffer))!=-1){size+=count;if(size>ModulePackage.LIMIT)throw new IOException("ZIP 过大。");output.write(buffer,0,count);}output.getFD().sync();
        }
    }
    static synchronized boolean removeThirdParty(String name){try{return service==null||service.deleteRemoteFile(name);}catch(RuntimeException error){return false;}}
    private static synchronized void publish(){
        if(service==null||local==null)return;
        try{SharedPreferences.Editor editor=service.getRemotePreferences("module_settings").edit().clear();
            for(Map.Entry<String,?> item:local.getAll().entrySet()){Object value=item.getValue();if(value instanceof String)editor.putString(item.getKey(),(String)value);}
            editor.putInt("schemaVersion",1);if(!editor.commit())android.util.Log.e("EModLoader","Framework config commit failed");
        }catch(RuntimeException error){android.util.Log.e("EModLoader","Framework config unavailable",error);}
    }
}

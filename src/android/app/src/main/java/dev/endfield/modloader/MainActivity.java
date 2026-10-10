package dev.endfield.modloader;
import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.net.Uri;
import android.widget.*;

public final class MainActivity extends Activity {
    private ModulePage page;
    @Override public void onCreate(Bundle state){super.onCreate(state);setTitle("Endfield Mod Loader");
        ScrollView scroll=new ScrollView(this);LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setPadding(20,20,20,20);scroll.addView(root);setContentView(scroll);
        ImageView logo=new ImageView(this);logo.setImageResource(R.drawable.logo);root.addView(logo,new LinearLayout.LayoutParams(160,160));
        TextView title=new TextView(this);title.setText("E MOD LOADER");title.setTextSize(25);root.addView(title);
        LinearLayout buttons=new LinearLayout(this);root.addView(buttons);
        Button offline=new Button(this);offline.setText("启动离线宿主");buttons.addView(offline);offline.setOnClickListener(v->startService(new Intent(this,OfflineHostService.class)));
        Button stop=new Button(this);stop.setText("停止宿主");buttons.addView(stop);stop.setOnClickListener(v->stopService(new Intent(this,OfflineHostService.class)));
        Button workshop=new Button(this);workshop.setText("创意工坊 ↗");root.addView(workshop);workshop.setOnClickListener(v->{try{startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse("https://146.235.16.65:8443/endfield/")));}catch(Exception error){Toast.makeText(this,error.getMessage(),Toast.LENGTH_SHORT).show();}});
        LinearLayout content=new LinearLayout(this);root.addView(content);page=new ModulePage(this,content);
    }
    @Override protected void onResume(){super.onResume();if(page!=null)page.render();}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(page!=null)page.onActivityResult(request,result,data);}
    @Override protected void onDestroy(){if(page!=null)page.close();super.onDestroy();}
}

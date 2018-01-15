package cn.poco.mycpp;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends Activity implements View.OnClickListener {

    // Used to load the 'native-lib' library on application startup.

    private Button btn_saveFrame, btn_addPngToMp4, btn_changeMp4Speed, btn_mp4Bleng;

    private String mfileDir = Environment.getExternalStorageDirectory().getAbsolutePath() + "/cbw/";
    private String mVideoOInName = "111.mp4";
    private String mVideoOutName = "2.mp4";

    private Handler mMainHandler;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = (TextView) findViewById(R.id.sample_text);
        tv.setText(VideoNativeUtils.stringFromJNI());

        File file = new File(mfileDir);
        if (!file.exists()) {
            file.mkdirs();
        }

        btn_saveFrame = (Button) findViewById(R.id.btn_saveFrame);
        btn_addPngToMp4 = (Button) findViewById(R.id.btn_addPngToMp4);
        btn_changeMp4Speed = (Button) findViewById(R.id.btn_changeMp4Speed);
        btn_mp4Bleng = (Button) findViewById(R.id.btn_mp4Bleng);

        btn_saveFrame.setOnClickListener(this);
        btn_addPngToMp4.setOnClickListener(this);
        btn_changeMp4Speed.setOnClickListener(this);
        btn_mp4Bleng.setOnClickListener(this);

        mMainHandler = new Handler(Looper.getMainLooper());

    }

    private int result;

    @Override
    public void onClick(final View v) {
        ((Button)v).setTextColor(Color.YELLOW);
        mMainHandler.post(new Runnable() {
            @Override
            public void run() {
                switch (v.getId()) {
                    case R.id.btn_saveFrame:
                        result = VideoNativeUtils.SaveFrameToBitmap(new VideoNativeUtils(), mfileDir + mVideoOInName, 20);
                        break;
                    case R.id.btn_addPngToMp4:
                        long s = System.currentTimeMillis();
                        result = VideoNativeUtils.AddPNGToMp4(mfileDir + mVideoOInName, mfileDir, mfileDir + mVideoOutName, mfileDir + "logo.png");
                        Log.i("bbb", "run: " + (System.currentTimeMillis() -s));
                        break;
                    case R.id.btn_changeMp4Speed:
                        result = VideoNativeUtils.ChangeVideoSpeed(mfileDir + mVideoOInName, mfileDir + "temp.mp4", mfileDir + mVideoOutName, 2f);
                        break;
                    case R.id.btn_mp4Bleng:
                        result = VideoNativeUtils.Blend2Video(mfileDir + "111.mp4", mfileDir + "112.mp4", mfileDir + "temp.h264", mfileDir + mVideoOutName, 0, 2);
                        break;
                }
                if (result >= 0) {
                    ((Button)v).setTextColor(Color.BLACK);
                } else {
                    ((Button)v).setTextColor(Color.RED);
                }
            }
        });

    }
}


package cn.poco.mycpp;

import android.graphics.Bitmap;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Created by cbw on 2017/3/29.
 */

public class VideoNativeUtils {


    static {
        System.loadLibrary("ffmpeg");
        System.loadLibrary("video");

    }

    public static native String stringFromJNI();

    public static native int SaveFrameToBitmap(VideoNativeUtils pObject, String pVideoFileName, int pNumOfFrames);

    private void saveFrameToPath(Bitmap bitmap, String pPath) {
        int BUFFER_SIZE = 1024 * 8;
        try {
            File file = new File(pPath);
            file.createNewFile();
            FileOutputStream fos = new FileOutputStream(file);
            final BufferedOutputStream bos = new BufferedOutputStream(fos, BUFFER_SIZE);
            bitmap.compress(Bitmap.CompressFormat.JPEG, 100, bos);
            bos.flush();
            bos.close();
            fos.close();

        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static native int AddPNGToMp4(String inputpath, String temppath, String outputpath, String uplayerpath);

    public static native int ChangeVideoSpeed(String inputpath, String temppath, String outputpath, float radio);

    public static native int Blend2Video(String MP4PATH1, String MP4PATH2, String OUTPUTH264,String OUTMP4PUTH, int type, int time);
}

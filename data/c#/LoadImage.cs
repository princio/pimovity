using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;
using UnityEngine.UI;

public class LoadImage : MonoBehaviour
{
    public string ip = "192.168.1.176";
    public int port = 8001;
    public RawImage rawimage;
    private Texture2D texture;

    private TcpClient client;
    private NetworkStream stream;
    private int STXi;
    private bool connected = false;
    private int size;
    private int toread;
    private int juread;
    private byte[] b = new byte[3000*1000];
    private int n = 0;
    private int step;
    private bool changingColor;
    private TcpClient linux;
    private TcpClient pi;
    private int im_cols = 16;
    private int im_rows;
    private AsyncCallback pi_callback;
    private AsyncCallback linux_callback;

    public byte[] STX { get; private set; }


    void Start()
    {
        texture = null;
        //new Texture2D(im_cols, im_rows);
        //rawimage.texture = texture;
        //linux_callback += 

        IPEndPoint ipe = new IPEndPoint(IPAddress.Parse(ip), port);

        connected = false;

        client = new TcpClient();

        UnityEngine.Debug.Log($"Waiting connection to: {ip}:{port}");

        client.BeginConnect(ip, port, new AsyncCallback(ConnectCallback), client);

        //UnityEngine.Debug.Log($"Connected!");


        //stream = pi.GetStream();
        //stream.Write(BitConverter.GetBytes(54), 0, 4);
        //var pi_cfg = new byte[12];
        //int r = stream.Read(pi_cfg, 0, 12);
        //if (r != 12) {
        //    UnityEngine.Debug.Log($"Error.");
        //}
        //Array.Copy(pi_cfg, STX, 4);
        //im_cols = BitConverter.ToInt32(pi_cfg, 4);
        //im_rows = BitConverter.ToInt32(pi_cfg, 8);

        //UnityEngine.Debug.Log($"STX=[{STXi}].\nResolution={im_cols}x{im_rows}.");


        toread = -1;
    }


    private void ConnectCallback(IAsyncResult ar)
    {
        client = (TcpClient)ar.AsyncState;
        client.EndConnect(ar);

        UnityEngine.Debug.Log($"Connected!");

        stream = client.GetStream();

        stream.Write(BitConverter.GetBytes(54), 0, 4);
        var pi_cfg = new byte[12];

        int r = stream.Read(pi_cfg, 0, 12);
        if (r != 12) {
            UnityEngine.Debug.Log($"Error ({r} != 12).");
        }

        STXi    = BitConverter.ToInt32(pi_cfg, 0);
        im_cols = BitConverter.ToInt32(pi_cfg, 4);
        im_rows = BitConverter.ToInt32(pi_cfg, 8);

        UnityEngine.Debug.Log($"Connected={client.Connected}! {im_cols}x{im_rows}.");
    }


    void Update()
    {
        if (Input.GetKeyDown(KeyCode.P)) {
            var c = rawimage.color;
            c.a += 0.1f;
            c.a = c.a > 1 ? 1 : c.a;
            rawimage.color = c;
        }
        else
        if (Input.GetKeyDown(KeyCode.O)) {
            var c = rawimage.color;
            c.a -= 0.1f;
            c.a = c.a < 0 ? 0 : c.a;
            rawimage.color = c;
        }

        if (!client.Connected || !stream.CanRead) return;

        if (texture == null) {
            texture = new Texture2D(im_cols, im_rows);
            rawimage.texture = texture;
        }

        var str = "";
        if(stream.DataAvailable) {
            if (toread > 0) {
                var toreadtmp = toread;
                toread -= stream.Read(b, size-toread, toread);
                str = $"toread={toreadtmp} -> {toread}";
            }
            else {
                byte[] bsize = new byte[8];
                stream.Read(bsize, 0, 8);
                int stx = BitConverter.ToInt32(bsize, 0);


                if (stx != STXi) {
                    UnityEngine.Debug.Log($"Wrong STX: {stx} != 27692\n");
                    return;
                }
                size = BitConverter.ToInt32(bsize, 4);

                UnityEngine.Debug.Log($"New photo stx={stx}, size={size}.");

                toread = size;
                if (stream.DataAvailable) {
                    toread -= stream.Read(b, 0, size);
                    str += $"\nsize={size}, toread={toread}";
                }
            }
            ++step;
        }
        if (toread == 0) {
            UnityEngine.Debug.Log($"Received photo {n}_{step}.");
            File.WriteAllBytes($"C:\\Users\\developer\\Desktop\\images\\img{n}_{step}.jpg", b);
            bool bo = texture.LoadImage(b);
            //str += $"\n--- canwrite={stream.CanWrite} --- canBeLoad={bo}";
            //stream.Write(new byte[4], 0, 4);
            toread = -1;
            n++;
            step = 0;
        }

    }
}

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
    public Camera main_camera;
    private Texture2D texture;
    public Material pupo_material;
    public GameObject man;



    private TcpClient client;
    private NetworkStream stream;
    private int STXi;
    private bool connected = false;
    private int size;
    private int type;
    private int toread;
    private int juread;
    private byte[] packets_buffer = new byte[3000*1000];
    private int nphoto = 0;
    private int nbbox;
    private int step;
    private bool changingColor;
    private TcpClient linux;
    private TcpClient pi;
    private int im_cols = 16;
    private int im_rows;
    private AsyncCallback pi_callback;
    private AsyncCallback linux_callback;
    private float alpha;
    private GameObject _m;

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

        toread = -1; nphoto = 0; nbbox = 0;

    }

    void Update()
    {
        if (Input.GetKeyDown(KeyCode.Space))
        {
            RaycastHit hit;
            UnityEngine.Debug.Log(Input.mousePosition);
            Ray ray = main_camera.ScreenPointToRay(Input.mousePosition);
            Vector3 hit2 = main_camera.ScreenToWorldPoint(Input.mousePosition);

            UnityEngine.Debug.DrawRay(main_camera.transform.position, ray.direction, Color.green, 1000);
            
            if (Physics.Raycast(ray, out hit))
            {
                
                UnityEngine.Debug.Log("obj: " + hit.transform.gameObject.name + " at " + hit.point);
                
                UnityEngine.Debug.DrawLine(ray.origin, hit.point);
                var m = Instantiate(man);
                m.transform.position = hit.point;
                m.transform.position += new Vector3(0f, 0.9f, 0f);
            }
            
        }
        if (Input.GetKeyDown(KeyCode.Z)) {
            var c = rawimage.color;
            c.a += 0.1f;
            c.a = c.a > 1 ? 1 : c.a;
            rawimage.color = c;
        }
        else
        if (Input.GetKeyDown(KeyCode.C)) {
            var c = rawimage.color;
            c.a -= 0.1f;
            c.a = c.a < 0 ? 0 : c.a;
            rawimage.color = c;
        }
        else
        if (Input.GetKeyDown(KeyCode.X))
        {
            var c = rawimage.color;
            alpha = c.a;
            c.a = 0;
            rawimage.color = c;
        } else
        if (Input.GetKeyUp(KeyCode.X))
        {
            var c = rawimage.color;
            c.a = alpha;
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
                toread -= stream.Read(packets_buffer, size-toread, toread);
                str = $"toread={toreadtmp} -> {toread}";
            }
            else
            if(toread == -1)
            {
                byte[] pk_head = new byte[12];
                int stx;

                stream.Read(pk_head, 0, 12);
                stx  = BitConverter.ToInt32(pk_head, 0);
                size = BitConverter.ToInt32(pk_head, 4);
                type = BitConverter.ToInt32(pk_head, 8);

                //UnityEngine.Debug.Log($"Packet: stx={stx}, type={type}|{(type == 0 ? nphoto : -1)}, size={size}, toread={toread}, read={size - toread}.");

                if (stx != STXi) {
                    int discarded = stream.Read(packets_buffer, 0, packets_buffer.Length);
                    int discarded_tot = discarded;
                    while (discarded > 0 && discarded - packets_buffer.Length == 0) {
                        discarded = stream.Read(packets_buffer, 0, packets_buffer.Length);
                        discarded_tot += discarded;
                    }
                    UnityEngine.Debug.Log($"Wrong STX: {stx} != 27692 (discarded {discarded_tot} bytes).");
                    size = 0;
                    return;
                }
                if (size <= 0)
                {
                    UnityEngine.Debug.Log($"Wrong Size: {size}.");
                    return;
                }
                if (type < 0 || type > 1)
                {
                    UnityEngine.Debug.Log($"Wrong Type: {type} not in [0,1].");
                    return;
                }
              
                toread = size;
                if (stream.DataAvailable) {
                    toread -= stream.Read(packets_buffer, 0, size);
                }

            }
            ++step;
        }
        if(toread == 0)
        {
            toread = -1;
            if (type == 0)
            {
                bool bo = texture.LoadImage(packets_buffer);
                nphoto++;
                step = 0;
                File.WriteAllBytes($"C:\\Users\\developer\\Desktop\\images\\img{nphoto}_{step}.jpg", packets_buffer);
                //UnityEngine.Debug.Log($"Photo {nphoto}_{step}.");
            } else
            if (type == 1)
            {
                float x = BitConverter.ToSingle(packets_buffer, 0);
                float y = BitConverter.ToSingle(packets_buffer, 4);
                float w = BitConverter.ToSingle(packets_buffer, 8);
                float h = BitConverter.ToSingle(packets_buffer, 12);
                float p = BitConverter.ToSingle(packets_buffer, 16);
                float o = BitConverter.ToSingle(packets_buffer, 20);
                int c   = BitConverter.ToInt32(packets_buffer, 24);

                Vector3 point = new Vector3();
                float cols = 1620;
                float rows = 1214;
                

                point.z = 0;
                point.x = cols * x;
                y = 1 - y - h / 2;
                y = y < 0 ? 0 : (y > 1 ? 1 : y);
                point.y = rows * y;

                MoveMan(point);

                nbbox++;

                UnityEngine.Debug.Log($"Bbox #{nbbox}: ({x}, {y}, {w}, {h}), p={p}, o={o}, c={c}.");


            }
        }
    }

    private void MoveMan(Vector3 point)
    {

        RaycastHit hit;
        Ray ray = main_camera.ScreenPointToRay(point);
    
        //UnityEngine.Debug.DrawRay(main_camera.transform.position, ray.direction, Color.green, 1000);

        if (Physics.Raycast(ray, out hit))
        {
            if(_m == null) _m = Instantiate(man);

            UnityEngine.Debug.Log("obj: " + hit.transform.gameObject.name + " at " + hit.point);
            UnityEngine.Debug.DrawLine(ray.origin, hit.point);

            _m.transform.position = hit.point;
        }
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
        if (r != 12)
        {
            UnityEngine.Debug.Log($"Error ({r} != 12).");
        }

        STXi = BitConverter.ToInt32(pi_cfg, 0);
        im_cols = BitConverter.ToInt32(pi_cfg, 4);
        im_rows = BitConverter.ToInt32(pi_cfg, 8);

        UnityEngine.Debug.Log($"Connected={client.Connected}! {im_cols}x{im_rows}.");
    }


}

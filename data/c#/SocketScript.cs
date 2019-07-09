using System;
using System.Collections;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.UI;
using HoloToolkit.Unity.SpatialMapping;
using HoloToolkit.Unity.InputModule;
using HoloToolkit.Unity;
using UnityEngine.XR.WSA.Input;

#if NETFX_CORE
using Stopwatch = System.Diagnostics.Stopwatch;
using System.Collections.Generic;
using Windows.Foundation;
using Windows.Graphics.Imaging;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.MediaProperties;
using Windows.Storage;
using Windows.Storage.Streams;
#endif

public class SocketScript : MonoBehaviour
{

    public GameObject bbox;
    public GameObject label;
    public GameObject button1;
    public GameObject button2;
    public GameObject statusText;
    public string ip = "192.168.1.141";
    public int port = 56789;
    private TextMesh labelTM;
    private int photoIndex = 0;
    private string line2;
    private string line1;
    private string line3;
    //private Renderer render;
    //private TextMesh textmesh;


#if NETFX_CORE
    private const int OVERHEAD_SIZE = 8; // STX:4,TYPE:1,INDEX:4,LENGTH:4,BUFFER:x,EXT:4
    private const int STX = 27692;//767590; //2,7,0,6,1,9,9,2=10,111,0,110,1,1001,1001,10=101110110110011001, int signed little order
    private const int ETX = 170807; //24121953
    private static byte[] STXb = BitConverter.GetBytes(STX);
    private static byte[] ETXb = BitConverter.GetBytes(ETX);
    private int startProcessing = 0;
    private IPEndPoint outEP;
    public SocketAsyncEventArgs recv_sae;
    private string[] categories;
    private uint buffer_size;
    private byte[] buffer;
    private byte[] rbuffer;
    private int timeouts;
    private Socket socket;
    private int socketIsBusy;
    private MediaFrameReader frameReader;
    private InMemoryRandomAccessStream inMemStream;
    private BitmapEncoder encoder;
    private Vector3 hitPosition;
    private Camera cameraMain;
    private SoftwareBitmap sbmp;
    private SocketError send_sae_error;
    private SocketError recv_sae_error;
    private GestureRecognizer recognizer;
    private int isEncoding;
    private int tapped;
    private GameObject photoBBox;
    private GameObject quad;
    private Renderer quadRenderer;
    private GameObject quadTM;
    private float distance;
    private int capturePhoto;
    private int processing;
    private CTEnum im_transform = CTEnum.SCALED;

    private int im_original_cols = 0;
    private int im_original_rows = 0;
    private int im_resized_cols = 0;
    private int im_resized_rows = 0;
    private int framerate = 0;
    private int im_resized_size = 0;
    private int im_resized_size_bytes = 0;
    private TextMesh st;
#endif

    /*
     * 896x504     5,15,20,24,30000/1001
     * 1280x720    5,15,20,24,30000/1001
     * 1344x756    5,15,20,24,30000/1001
     * 1408x792    5,15,20,24,30000/1001
    */


    public async void ButtonConnectDown()
    {
        Debug.Log("connect down.");
        line3 = "connect button down.";

#if NETFX_CORE
        await StartSocket();
#endif
    }


    public void ButtonDisconnectDown()
    {
#if NETFX_CORE
        socket.Shutdown(SocketShutdown.Both);
        socket.Dispose();
        socket = null;
#endif
    }
#if NETFX_CORE
    void UpdateStatus(int l = 0, string s = "")
    {
        switch(l) {
            case 1:
                line1 = s;
                break;
            case 2:
                line2 = s;
                break;
            case 3:
                line3 = s;
                break;
            default:
                break;
        }

        st.text = line1 + "\n" + line2 + "\n" + line3;
    }

    async void readIpFromFile()
    {
        try {
            var lib = await StorageLibrary.GetLibraryAsync(KnownLibraryId.Pictures);
            var file = await lib.SaveFolder.GetFileAsync("ip.txt");
            ip = await FileIO.ReadTextAsync(file);

            Debug.Log($"IP is: {ip}.");
        }
        catch (Exception e) {
            Debug.Log($"IP unretrievable: {e}");
        }
    }

    async void Start()
    {
        st = statusText.GetComponent<TextMesh>();
        statusText.transform.parent.transform.position += new Vector3(-1, 1, 0);

        line1 = "#";
        line2 = "#";
        line3 = "#";
        UpdateStatus();


        framerate = 5;
        im_original_cols = 896;
        im_original_rows = 504;
        im_resized_cols = 416;
        im_resized_rows = (int)(((float)im_resized_cols / im_original_cols) * im_original_rows);
        im_resized_size = im_resized_rows * im_resized_cols;
        im_resized_size_bytes = im_resized_size * 3;
        ip = "192.168.1.141";
        socket = null;
        tapped = 0;
        startProcessing = 0;
        rbuffer = new byte[256];


        line2 = "starting capturer...";
        await StartCapturer();

        /*await StartSocket();
        startProcessing = 0;
        await frameReader.StartAsync();
        Debug.Log("Socket and FrameReader started!!!");*/


        line1 = "start completed.";
    }

    async void Update()
    {
        UpdateStatus();
        try {
            if(socket != null) {
                if (socket.Connected == false) {
                    socket.Dispose();
                    await StartSocket();
                }
                if (startProcessing == 1 && 0 == Interlocked.CompareExchange(ref processing, 1, 0)) {
                    processing = 1;
                    StartCoroutine("MyCoroutine");
                }
            }
        }
        catch (Exception e) {
            Debug.Log($"Something wrong in update: {e}");
        }
    }

    private void TapHandler(TappedEventArgs obj)
    {
        if (obj.tapCount != 2) return;

        if (line3 == "." || line3 == "..") line3 += ".";
        else line3 = ".";
        UpdateStatus();

        if (1 == Interlocked.CompareExchange(ref capturePhoto, 1, 0)) {
            return;
        }

        hitPosition = GazeManager.Instance.HitPosition;
        cameraMain = CameraCache.Main;//GazeManager.Instance.HitNormal;

        photoBBox = Instantiate(bbox, hitPosition, Quaternion.identity) as GameObject;

        distance = Vector3.Distance(hitPosition, cameraMain.transform.position);

        photoBBox.transform.localScale = 0.6f * photoBBox.transform.localScale.Mul(new Vector3(distance, distance, distance));
        photoBBox.transform.position = hitPosition;
        photoBBox.transform.forward = cameraMain.transform.forward;
        photoBBox.transform.Rotate(-photoBBox.transform.eulerAngles.x, 0, -photoBBox.transform.eulerAngles.z);

        quad = photoBBox.transform.Find("Quad").gameObject;
        quadRenderer = quad.GetComponent<Renderer>() as Renderer;
        label = photoBBox.transform.Find("Label").gameObject;
        labelTM = label.GetComponent<TextMesh>();
        labelTM.text = "trying...";
    }

    private void OnFrameArrived(MediaFrameReader sender, MediaFrameArrivedEventArgs args)
    {
        if (line2 == "." || line2 == "..") line2 += ".";
        else line2 = ".";
        UpdateStatus();

        if (0 == capturePhoto) {
            return;
        }

        try {
            using (var frame = sender.TryAcquireLatestFrame()) {
                if (frame != null && frame.VideoMediaFrame != null) {
                    sbmp = frame.VideoMediaFrame.SoftwareBitmap;// SoftwareBitmap.Convert(frame.VideoMediaFrame.SoftwareBitmap, BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore);
                    Interlocked.Exchange(ref startProcessing, 1);
                }
            }
        }
        catch (Exception e) {
            Debug.Log("OnFramerArrived: " + e);
        }
    }

    IEnumerator MyCoroutine()
    {
        bool isAsync;
        int size;

        Code();

        Interlocked.Exchange(ref isEncoding, 1);
        var task = encoder.FlushAsync();
        task.Completed += new AsyncActionCompletedHandler((IAsyncAction source, AsyncStatus status) => {
            Interlocked.Exchange(ref isEncoding, 0);
        });
        do {
            yield return null;
        } while (1 == Interlocked.CompareExchange(ref isEncoding, 0, 0));
        sbmp = null;

        yield return null;

        size = PreparePacket();

        yield return null;

    #region sending
        int sent;
        using (var send_sae = new SocketAsyncEventArgs()) {

            isAsync = false;
            try {
                send_sae.Completed += transfer_callback;
                try {
                    send_sae.SetBuffer(buffer, 0, size);
                }
                catch {
                    Debug.Log("Error in SSetBuffer: 205.");
                }
                Interlocked.Exchange(ref socketIsBusy, 1);
                isAsync = socket.SendAsync(send_sae);
            }
            catch (SocketException e) {
                Debug.LogFormat("Receiving timeout." + e.SocketErrorCode.ToString());
                yield break;
            }
            do {
                yield return null;
            } while (1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0));
            if (!isAsync) yield break;

            if (send_sae_error != SocketError.Success) {
                Debug.Log("Socket::Send: error = " + send_sae_error);
                yield break;
            }
            sent = send_sae.BytesTransferred;
        }
        if (sent < size) {
            Debug.LogFormat("Error not sent {0} bytes", size - sent);
            yield break;
        }
        inMemStream.Dispose();
    #endregion

    #region receiving
        isAsync = false;
        try {
            Interlocked.Exchange(ref socketIsBusy, 1);
            isAsync = socket.ReceiveAsync(recv_sae);
        }
        catch (Exception e) {
            Debug.Log(e.Message);
        }
        do {
            yield return null;
        } while (1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0));
        if (!isAsync) yield break;
        if (recv_sae_error != SocketError.Success) {
            Debug.Log("Socket::Send: error = " + recv_sae_error);
            yield break;
        }
    #endregion

        HandleBBox(recv_sae.BytesTransferred);

        ++photoIndex;

        Interlocked.Exchange(ref capturePhoto, 0);
        Interlocked.Exchange(ref startProcessing, 0);
        Interlocked.Exchange(ref processing, 0);

        yield return null;
    }

    private void HandleBBox(int rl)
    {
        int nbbox, stx;

        if (rl == 0) {
            Debug.Log($"Too few data received ({rl}).");
            return;
        }
        try {
            BinaryReader breader;
            try {
                breader = new BinaryReader(new MemoryStream(recv_sae.Buffer));
            }
            catch (Exception e) {
                Debug.Log("HandleBBox: " + e);
                return;
            }
            stx = breader.ReadInt32();
            nbbox = breader.ReadInt32();

            if (stx != STX) {
                Debug.Log(string.Format("wrong STX ({0,5} != {1,5}.", stx, STX));
            }
            else
            if (nbbox > 0) {
                for (int nb = 0; nb < nbbox; ++nb) {
                    float x = breader.ReadSingle(); //is the x coordinate of the bbox center
                    float y = breader.ReadSingle(); //is the y coordinate of the bbox center
                    float w = breader.ReadSingle(); //is the width of the bbox
                    float h = breader.ReadSingle(); //is the height of the bbox
                    float o = breader.ReadSingle();
                    float p = breader.ReadSingle();
                    int cindex = breader.ReadInt32();

                    Debug.LogFormat("[{0,5}:{1}]:\tx={2,6:F4}, y={3,6:F4}, w={4,6:F4}, h={5,6:F4}, o={6,6:F4}, p={7,6:F4}, name#{8,2}=" + categories[cindex] + ".", photoIndex, nb, x, y, w, h, o, p, cindex);

                    GenBBox(w, h, x, y, p, cindex);
                }
            }
            else
            if (nbbox == 0) {
                Destroy(photoBBox);
                Debug.Log("[{0:5}] No bbox found.");
            }
            else {
                Debug.LogFormat("[{0:5}]: error during inference.", photoIndex);
                //iros.Remove(index);
            }
        }
        catch {
            Debug.Log("HandleBBox error.");
        }
        finally {
            Interlocked.Exchange(ref tapped, 0);
            Destroy(photoBBox);
        }
    }

    private void GenBBox(float w, float h, float x, float y, float p, int cindex)
    {
        var z = distance;

        var box = Instantiate(bbox, hitPosition, Quaternion.identity) as GameObject;

        var quat = cameraMain.transform.rotation;
        box.transform.forward = quat * Vector3.forward;
        box.transform.Rotate(-box.transform.eulerAngles.x, 0, -box.transform.eulerAngles.z); // makes bbox vertical

        var quad = box.transform.Find("Quad").gameObject;
        var quadRenderer = quad.GetComponent<Renderer>() as Renderer;
        Bounds quadBounds = quadRenderer.bounds;

        float zx = z;
        float zy = z;// < 1f ? z * 1.2f : z * 0.7f;
        w = w * z; // (w < 0.1f ? 0.1f : w) * z;
        h = h * zy; // (h < 0.1f ? 0.1f : h) * (z < 1f ? z : z * 0.7f);
        //x *= z;
        //y *= z * 0.7f;

        quad.transform.localScale = new Vector3(w, h, 1);

        quadBounds.size.Set(w, y, quadBounds.size.z);

        //box.transform.position -= new Vector3(x, y, 0);

        Debug.Log($"{z}\n{x}, {y}, {w}, {h}\n{quadBounds.size}\n{quadBounds.size.normalized}\n{quadBounds}\n{quad.transform.localScale}");

        var label = box.transform.Find("Label").gameObject.GetComponent<TextMesh>();
        label.text = categories[cindex] + $"[{Math.Ceiling(p * 100f)}%]";
    }

    private int PreparePacket()
    {
        Stream stream;
        int size = (int)inMemStream.Size - 54;

        stream = inMemStream.AsStreamForRead();
        stream.Seek(54, SeekOrigin.Begin);

        int read_size = stream.Read(buffer, 8, size);
        if (size != read_size) { throw new Exception(string.Format("Reading error: read {0} on {1}.", read_size, size)); }

        Array.Copy(BitConverter.GetBytes(STX), 0, buffer, 0, 4);
        Array.Copy(BitConverter.GetBytes(size), 0, buffer, 4, 4);

        return size + 8;
    }

    private async Task StartSocket()
    {
        IPAddress ipa = IPAddress.Parse(ip);
        outEP = new IPEndPoint(ipa, port);
        recv_sae = new SocketAsyncEventArgs();
        recv_sae.Completed += transfer_callback;
        socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        Interlocked.Exchange(ref socketIsBusy, 0);

        while (true) {
            try {
                bool isAsync = false;
                Debug.Log($"trying to connect to {outEP}...");
                line1 = $"trying to connect to {outEP}...";
                var connect_sae = new SocketAsyncEventArgs();

                Interlocked.Exchange(ref socketIsBusy, 1);
                connect_sae.Completed += connect_callback; ///socket_(s, e) => autoResetEvent.Set();
                connect_sae.RemoteEndPoint = outEP;
                isAsync = socket.ConnectAsync(connect_sae);
                

                while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
                    Debug.Log("waiting");
                    await Task.Delay(1000);
                }

                if (socket.Connected) break;
                else {
                    Debug.Log($"not connected [async={isAsync}].");
                    await Task.Delay(500);
                }
            }
            catch (SocketException e) {
                line1 = $"failed to connect to {outEP}!";
                Debug.Log($"failed: {e}\n");
            }
        }

        line1 = $"connected to {outEP}.";
        await SendConfig();



        recognizer = new GestureRecognizer();
        recognizer.SetRecognizableGestures(GestureSettings.Tap);
        recognizer.Tapped += TapHandler;
        recognizer.StartCapturingGestures();


        enabled = true;
    }

    private void connect_callback(object sender, SocketAsyncEventArgs e)
    {
        Interlocked.Exchange(ref socketIsBusy, 0);
        Debug.Log(e.SocketError);
        Debug.Log(e.AcceptSocket);
    }

    private void transfer_callback(object sender, SocketAsyncEventArgs e)
    {
        //Debug.LogFormat("Args_Completed:\t{0} => {1}   [{2}].", e.LastOperation.ToString(), e.SocketError.ToString(), e.BytesTransferred);
        if (e.LastOperation == SocketAsyncOperation.Send)
            send_sae_error = e.SocketError;
        if (e.LastOperation == SocketAsyncOperation.Receive)
            recv_sae_error = e.SocketError;
        Interlocked.Exchange(ref socketIsBusy, 0);
    }

    private async Task SendConfig()
    {
        int r;
        try {
            line1 = "sending config...";

            byte[] config_buffer = new byte[16];

            Array.Copy(BitConverter.GetBytes((int)im_original_cols), 0, config_buffer, 0, 4);
            Array.Copy(BitConverter.GetBytes((int)im_original_rows), 0, config_buffer, 4, 4);
            Array.Copy(BitConverter.GetBytes((int)im_resized_cols), 0, config_buffer, 8, 4);
            Array.Copy(BitConverter.GetBytes((int)im_resized_rows), 0, config_buffer, 12, 4);

            await SendAsync(config_buffer, config_buffer.Length);
            Debug.Log("Sent config.");
            line1 = "config sent.";

            config_buffer[0] = 0;

            recv_sae.SetBuffer(config_buffer, 0, 12);
            r = await Receive();

            int ln = 0, nn;
            if (BitConverter.ToInt32(config_buffer, 0) == STX) {
                ln = BitConverter.ToInt32(config_buffer, 4);
                nn = BitConverter.ToInt32(config_buffer, 8);
            }
            else {
                throw new SocketException();
            }
            Debug.Log($"Recv ln={ln} and nn={nn}.");
            line1 = $"Classes: l={ln}, n={nn}.";

            byte[] cbuf = new byte[ln];
            recv_sae.SetBuffer(cbuf, 0, ln);
            if (await Receive() < ln) throw new Exception($"Not all class string received (missing {ln} bytes).");

            BinaryReader br = new BinaryReader(new MemoryStream(cbuf));

            categories = new string[nn];

            int ci = 0;
            StringBuilder sb = new StringBuilder("");
            for (int i = 0; i < ln; i++) {
                char c = '_';
                try {
                    c = br.ReadChar();
                    if (c == '\0') {
                        categories[ci++] = sb.ToString();
                        sb.Clear();
                    }
                    else {
                        sb.Append(c);
                    }
                }
                catch (Exception e) {
                    Debug.Log($"Some error in classes strings: {e}");
                }
            }

            buffer_size = 8 + (uint)im_resized_size_bytes;
            buffer = new byte[buffer_size + 10];
            rbuffer = new byte[160];

            try {
                recv_sae.SetBuffer(rbuffer, 0, 150);
            }
            catch {
                Debug.Log("Error in RSetBuffer: 482.");
            }

            Array.Copy(STXb, buffer, 4);

            Debug.Log($"Buffer length: {buffer.Length}\n");

        }
        catch (Exception e) {
            Debug.Log($"Something happen.\n{e}");
            line1 = $"Error in config.";
        }
        line1 = $"Config received.";
    }

    public async Task<int> SendAsync(byte[] b, int l)
    {
        try {
            bool isAsync = false;
            var send_sae = new SocketAsyncEventArgs();
            send_sae.Completed += transfer_callback;
            try {
                send_sae.SetBuffer(b, 0, l);
            }
            catch {
                Debug.Log("Error in SSetBuffer: 505.");
            }
            Interlocked.Exchange(ref socketIsBusy, 1);
            isAsync = socket.SendAsync(send_sae);
            while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
                await Task.Delay(5);
            }
            if (send_sae.SocketError != SocketError.Success) Debug.Log("Socket::Send: error = " + send_sae.SocketError);
            int sl = send_sae.BytesTransferred;
            send_sae.Dispose();
            return sl;
        }
        catch (Exception e) {
            Debug.Log("Socket::Send:\t" + e.Message);
        }
        return -1;
    }

    public async Task<int> Receive()
    {
        try {
            bool isAsync = false;
            Interlocked.Exchange(ref socketIsBusy, 1);
            isAsync = socket.ReceiveAsync(recv_sae);
            while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
                await Task.Delay(5);
            }
            return recv_sae.BytesTransferred;
        }
        catch (Exception e) {
            Debug.Log(e.Message);
        }
        return -1;
    }

    private async Task StartCapturer()
    {
        MediaFrameSource mediaFrameSource;
        var allGroups = await MediaFrameSourceGroup.FindAllAsync();
        if (allGroups.Count <= 0) {
            line2 = "no media!";
        }
        var mediaCapture = new MediaCapture();
        var settings = new MediaCaptureInitializationSettings
        {
            SourceGroup = allGroups[0],
            SharingMode = MediaCaptureSharingMode.SharedReadOnly,
            StreamingCaptureMode = StreamingCaptureMode.Video,
            MemoryPreference = MediaCaptureMemoryPreference.Cpu
        };

        await mediaCapture.InitializeAsync(settings);

        //render.material.color = new Color(0, 0, 0.5f);

        string formats = "";

        mediaFrameSource = mediaCapture.FrameSources.Values.Single(x => x.Info.MediaStreamType == MediaStreamType.VideoRecord);
        try {
            MediaFrameFormat targetResFormat = null;
            foreach (var f in mediaFrameSource.SupportedFormats.OrderBy(x => x.VideoFormat.Width * x.VideoFormat.Height)) {
                //textmesh.text = string.Format("{0}x{1} {2}/{3}", f.VideoFormat.Width, f.VideoFormat.Height, f.FrameRate.Numerator, f.FrameRate.Denominator);
                if (f.VideoFormat.Width == im_original_cols && f.VideoFormat.Height == im_original_rows && f.FrameRate.Numerator == framerate) {
                    targetResFormat = f;
                }
                formats += $"{f.VideoFormat.Width}x{f.VideoFormat.Height} at {f.FrameRate.Numerator}/{f.FrameRate.Denominator}\n";
            }

            Debug.Log(formats);

            await mediaFrameSource.SetFormatAsync(targetResFormat);
        }
        catch {
            line2 = "no right format!";
        }

        try {
            frameReader = await mediaCapture.CreateFrameReaderAsync(mediaFrameSource, MediaEncodingSubtypes.Bgra8);
            frameReader.AcquisitionMode = MediaFrameReaderAcquisitionMode.Realtime;
            frameReader.FrameArrived += OnFrameArrived;
        }
        catch {
            line2 = "no event handling!";
        }

        line2 = "capturer started.";
    }

    private void Code()
    {
        inMemStream = new InMemoryRandomAccessStream();
#if CODING_AWAIT
        encoder = await BitmapEncoder.CreateAsync(config.EncoderGuid, inMemStream);
#else
        encoder = BitmapEncoder.CreateAsync(BitmapEncoder.BmpEncoderId, inMemStream).AsTask().GetAwaiter().GetResult();
#endif
        encoder.BitmapTransform.Flip = BitmapFlip.Vertical;
        switch (im_transform) {
            case CTEnum.CROPPED:
                encoder.BitmapTransform.Bounds = new BitmapBounds()
                {
                    X = (uint)(im_original_cols - im_resized_cols) >> 1, //(config.OriginalResolution.Width - config.FinalResolution.Width) >> 1,
                    Y = (uint)(im_original_rows - im_resized_rows) >> 1,      // (config.OriginalResolution.Height - config.FinalResolution.Height) >> 1,
                    Width = (uint)im_resized_cols, //config.FinalResolution.Width,
                    Height = (uint)im_resized_rows //config.FinalResolution.Height
                };
                break;
            case CTEnum.SCALED:
                encoder.BitmapTransform.InterpolationMode = BitmapInterpolationMode.Linear;
                encoder.BitmapTransform.ScaledWidth = (uint)im_resized_cols;  //config.FinalResolution.Width;
                encoder.BitmapTransform.ScaledHeight = (uint)im_resized_rows; //config.FinalResolution.Height;
                break;
            case CTEnum.SQUARED:
                uint h = (uint)im_resized_rows; // config.OriginalResolution.Height;
                encoder.BitmapTransform.Bounds = new BitmapBounds()
                {
                    X = ((uint)im_resized_cols - h) >> 1,
                    Y = h,
                    Width = h,
                    Height = h
                };
                encoder.BitmapTransform.InterpolationMode = BitmapInterpolationMode.Linear;
                encoder.BitmapTransform.ScaledWidth = (uint)im_resized_cols; // config.FinalResolution.Width;
                encoder.BitmapTransform.ScaledHeight = (uint)im_resized_rows;  //config.FinalResolution.Height;
                break;
        }
        sbmp = SoftwareBitmap.Convert(sbmp, BitmapPixelFormat.Rgba8, BitmapAlphaMode.Ignore);
        encoder.SetSoftwareBitmap(sbmp);
    }
#endif

}

#if NETFX_CORE

public enum CTEnum
{
    NONE, CROPPED, SCALED, SQUARED
}
#endif
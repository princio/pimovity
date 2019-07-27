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
using HoloToolkit.Examples.InteractiveElements;

#if NETFX_CORE
using Windows.Media.Effects;
using Windows.Foundation.Collections;
using Windows.Media;
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
    public GameObject button1;
    public GameObject button2;
    public GameObject btn_show_menu;
    public GameObject menu;
    public GameObject statusText;
    public GameObject IpPortDigits;
    private TextMesh[] textmesh_digits;
    private int digit_index = 0;

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
    private const int OVERHEAD_SIZE = 12; // STX:4,TYPE:1,INDEX:4,LENGTH:4,BUFFER:x,EXT:4
    private const int STX = 27692;//767590; //2,7,0,6,1,9,9,2=10,111,0,110,1,1001,1001,10=101110110110011001, int signed little order
    private static byte[] STXb = BitConverter.GetBytes(STX);
    private IPEndPoint outEP;
    public SocketAsyncEventArgs recv_sae;
    private string[] categories;
    private uint buffer_size;
    private byte[] buffer;
    private byte[] rbuffer;
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
    private int tapped;
    private GameObject photoBBox;
    private GameObject quad;
    private Renderer quadRenderer;
    private GameObject label;
    private float distance;
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
    private SocketAsyncEventArgs send_sae;
    private SocketStatus socketStatus;
    private MediaCapture mediaCapture;
    private int requestNN;

    const int NOT_READY = 0;
    const int WAIT_FRAME = 1;
    const int SAVE_FRAME = 2;
    const int TO_ELABORATE = 3;
    const int ELABORATING = 4;

    public IVideoEffectDefinition videoEffect { get; private set; }

    private IMediaExtension videoEffectPreview;
    private IMediaExtension videoEffectRecord;

    enum SocketStatus
    {
        TRY,
        CONNECTED,
        NONE
    }

#endif

    /*
     * 896x504     5,15,20,24,30000/1001
     * 1280x720    5,15,20,24,30000/1001
     * 1344x756    5,15,20,24,30000/1001
     * 1408x792    5,15,20,24,30000/1001
    */


    public void ButtonMenuDown()
    {
        var label = btn_show_menu.GetComponent<LabelTheme>();
        if (menu.activeSelf == false) {
            label.Default = "Hide Menu";
            menu.SetActive(true);
            menu.transform.position = CameraCache.Main.transform.position + CameraCache.Main.transform.forward * 2 + new Vector3(0.1f, 0, 0);
            menu.transform.forward = CameraCache.Main.transform.forward;
        } else {
            label.Default = "Show Menu";
            menu.SetActive(false);
        }

        Debug.Log($"ButtonMenuDown.");
        line3 = $"ButtonMenuDown.";
    }

    private void DisposeSocket()
    {
#if !UNITY_EDITOR
        try {
            socket.Shutdown(SocketShutdown.Both);
        }
        catch { }
        try {
            socket.Dispose();
        }
        catch { }
        socket = null;

        socketStatus = SocketStatus.NONE;
        Button1UpdateString($"Connect to\n{ip}:{port}");
#endif
    }

    public void ButtonConnectDown()
    {

#if NETFX_CORE
        string b_str = "";
        ///*await*/ StartSocket();
        try {
            switch (socketStatus) {
                case SocketStatus.NONE:
                    socketStatus = SocketStatus.TRY;
                    StartCoroutine("CoroutineStartSocket");
                    b_str = $"STOP\n(trying to {ip}:{port})";
                    break;
                case SocketStatus.TRY:
                    StopCoroutine("CoroutineStartSocket");
                    Interlocked.Exchange(ref processing, NOT_READY);
                    socketStatus = SocketStatus.NONE;
                    socket?.Dispose();
                    socket = null;
                    b_str = $"Connect to\n{ip}:{port}";
                    break;
                case SocketStatus.CONNECTED:
                    Interlocked.Exchange(ref processing, NOT_READY);
                    socketStatus = SocketStatus.NONE;
                    socket.Shutdown(SocketShutdown.Both);
                    socket?.Dispose();
                    socket = null;
                    b_str = $"Connect to\n{ip}:{port}";
                    break;
            }
        } catch (Exception e) {
            Debug.Log($"Button Connect: {e}");
        }

        Button1UpdateString(b_str);
#endif

        Debug.Log($"connect down: {ip}:{port}");
        line3 = $"connect button down: {ip}:{port}";
    }

    private void Button1UpdateString(string s)
    {
        button1.GetComponent<LabelTheme>().Default = s;
        button1.transform.GetChild(1).GetChild(1).GetComponent<TextMesh>().text = s;
    }

    public void ButtonIpLeftDown()
    {
        textmesh_digits[digit_index].color = Color.white;
        digit_index = --digit_index == -1 ? 16 : digit_index;
        textmesh_digits[digit_index].color = Color.red;
        Debug.Log("ButtonIpLeftDown.");
        line3 = "ButtonIpLeftDown.";
    }

    public void ButtonIpRightDown()
    {
        textmesh_digits[digit_index].color = Color.white;
        digit_index = ++digit_index > 16 ? 0 : digit_index;
        textmesh_digits[digit_index].color = Color.red;
        Debug.Log("ButtonIpRightDown.");
        line3 = "ButtonIpRightDown.";
    }

    string GetDigit(int idx)
    {
        return textmesh_digits[idx].text;
    }

    int DigitgetMax(bool plus = true)
    {
        int max = 9;
        if (digit_index < 12) {
            int digit_group_start = digit_index / 3 * 3;
            if (digit_index % 3 == 0) max = 2;
            else
            if (GetDigit(digit_group_start) == "2") {
                if (digit_index % 3 == 1) max = 5;
                else
                if (digit_index % 3 == 2 && GetDigit(digit_group_start + 1) == "5") max = 5;
            }
        }
        return max;
    }
    void TextMesh2Ip()
    {
#if !UNITY_EDITOR
        IPAddress ipa;
        string __ip = "";
        for (int i = 0; i < 12; i+=3) {
            int isubip = int.Parse(textmesh_digits[i].text + textmesh_digits[i + 1].text + textmesh_digits[i + 2].text);
            __ip += isubip.ToString() + ".";
        }
        __ip = __ip.Substring(0, __ip.Length - 1);

        if(IPAddress.TryParse(__ip, out ipa) == false) {
            line1 = $"Wrong ip address: {__ip}.";
            return;
        }
        line1 = "Correct ip address.";
        ip = __ip;

        var sport = "";
        for (int i = 12; i <= 16; i++) {
            sport += textmesh_digits[i].text;
        }
        if (int.TryParse(sport, out int _port) == false) {
            line1 = $"Wrong port: {port}.";
            return;
        }

        port = _port;

        button1.GetComponent<LabelTheme>().Default = $"Connect to\n{ip}:{port}";
        button1.transform.GetChild(1).GetChild(1).GetComponent<TextMesh>().text = $"Connect to\n{ip}:{port}";
#endif
    }

    void Ip2TextMesh()
    {
        string ip_digits = "";
        foreach(var subip in ip.Split('.')) {
            ip_digits += new String('0', 3 - subip.Length) + subip;
        }
        Debug.Log("###########\t" + ip_digits);
        for (int i = 0; i < ip_digits.Length; i++) {
            textmesh_digits[i].text = ip_digits[i].ToString();
        }

        var sport = port.ToString();
        for (int i = 0; i < sport.Length; i++) {
            textmesh_digits[i + 12].text = sport[i].ToString();
        }
    }

    public void ButtonIpPlusDown()
    {
        int max = DigitgetMax();

        int n = Int32.Parse(textmesh_digits[digit_index].text);
        n = ++n > max ? 0 : n;
        textmesh_digits[digit_index].text = n.ToString();

        TextMesh2Ip();

        Debug.Log("ButtonIpPlusDown.");
        line3 = "ButtonIpPlusDown.";
    }

    public void ButtonIpMinusDown()
    {
        int max = DigitgetMax(false);

        int n = Int32.Parse(textmesh_digits[digit_index].text);
        n = --n < 0 ? max : n;
        textmesh_digits[digit_index].text = n.ToString();

        TextMesh2Ip();


        Debug.Log("ButtonIpMinusDown.");
        line3 = "ButtonIpMinusDown.";
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

    async void Start()
    {
        socketStatus = SocketStatus.NONE;

        st = statusText.GetComponent<TextMesh>();
        statusText.transform.parent.transform.position += new Vector3(-1, 1, 0);

        line1 = "#";
        line2 = "#";
        line3 = "#";
        UpdateStatus();

        textmesh_digits = new TextMesh[IpPortDigits.transform.childCount];
        string names = "";
        for (int i = 0; i < IpPortDigits.transform.childCount; i++) {
            textmesh_digits[i] = IpPortDigits.transform.GetChild(i).GetComponent<TextMesh>();
            names += textmesh_digits[i].name + ",";
        }
        Debug.Log(names);
        textmesh_digits[0].color = Color.red;


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
        rbuffer = new byte[256];

        Ip2TextMesh();
        Button1UpdateString($"Connect to\n{ip}:{port}");

        processing = NOT_READY;

        await StartCapturer();

        //mediaCapture = new MediaCapture();
        //await mediaCapture.InitializeAsync();
        //mediaCapture.Failed += MediaCapture_Failed;

        line1 = "start completed.";
    }

    private void MediaCapture_Failed(MediaCapture sender, MediaCaptureFailedEventArgs errorEventArgs)
    {
        throw new NotImplementedException();
    }

    void Update()
    {
        UpdateStatus();

        if(socket != null && !socket.Connected) {
            Interlocked.Exchange(ref processing, NOT_READY);
            socket.Dispose();
            socketStatus = SocketStatus.NONE;
        }

        if (TO_ELABORATE == Interlocked.CompareExchange(ref processing, ELABORATING, TO_ELABORATE)) {
            StartCoroutine("CoroutineAll", requestNN == 1);
        }
    }

    private void BBoxInit()
    {
        hitPosition = GazeManager.Instance.HitPosition;
        cameraMain = CameraCache.Main;//GazeManager.Instance.HitNormal;

        distance = Vector3.Distance(hitPosition, cameraMain.transform.position);
        photoBBox = Instantiate(bbox, hitPosition, Quaternion.identity) as GameObject;
        photoBBox.transform.localScale = 0.2f * photoBBox.transform.localScale.Mul(new Vector3(distance, distance, distance));
        photoBBox.transform.position = hitPosition;
        photoBBox.transform.forward = cameraMain.transform.forward;
        photoBBox.transform.Rotate(-photoBBox.transform.eulerAngles.x, 0, -photoBBox.transform.eulerAngles.z);
        quad = photoBBox.transform.Find("BBoxQuad").gameObject;
        quadRenderer = quad.GetComponent<Renderer>() as Renderer;
        label = photoBBox.transform.Find("BBoxLabel").gameObject;
        labelTM = label.GetComponent<TextMesh>();
        labelTM.text = "trying...";
    }

    private void TapHandler(TappedEventArgs obj)
    {
        var hitObj = GazeManager.Instance.HitObject;
        string name = (hitObj == null) ? "null" : hitObj.name;

        if(hitObj == null) {
            line3 = "hit null";
            Interlocked.CompareExchange(ref requestNN, 1, 0);
        }
        else
        if(hitObj.tag.IndexOf("SocketUI") < 0)
        {

            if(hitObj.transform.parent.name.IndexOf("BBox", 0) >= 0) {
                DestroyImmediate(hitObj.transform.parent);
            }

            Interlocked.CompareExchange(ref requestNN, 1, 0);

            line3 = name;

            Debug.Log(hitObj.tag);
        }
    }

    private void OnFrameArrived(MediaFrameReader sender, MediaFrameArrivedEventArgs args)
    {
        line3 = "new frame";
        if (WAIT_FRAME == Interlocked.CompareExchange(ref processing, SAVE_FRAME, WAIT_FRAME)) {
            try {
                using (var frame = sender.TryAcquireLatestFrame()) {
                    if (frame != null && frame.VideoMediaFrame != null) {
                        sbmp = frame.VideoMediaFrame.SoftwareBitmap;
                        Interlocked.Exchange(ref processing, TO_ELABORATE);
                    }
                }
            }
            catch (Exception e) {
                Debug.Log("OnFramerArrived: " + e);
            }
        }
    }

    IEnumerator CoroutineEncode(bool isNN)
    {
        inMemStream = new InMemoryRandomAccessStream();
        var task1 = BitmapEncoder.CreateAsync(isNN ? BitmapEncoder.BmpEncoderId : BitmapEncoder.JpegEncoderId, inMemStream);

        while (task1.Status == AsyncStatus.Started) {
            yield return new WaitForEndOfFrame();
        }
        if (task1.Status != AsyncStatus.Completed) {
            encoder = null;
            line3 = $"Failed to create encoder: {task1.ErrorCode}.";
            yield break;
        }

        encoder = task1.GetResults();

        if(isNN) {
            PrepareEncoderNN();
        } else {
            encoder.SetSoftwareBitmap(sbmp);
        }

        var task2 = encoder.FlushAsync();
        while (task2.Status == AsyncStatus.Started) {
            yield return new WaitForEndOfFrame();
        }
        if (task1.Status != AsyncStatus.Completed) {
            inMemStream = null;
        }
    }

    IEnumerator CoroutineAll(bool isNN)
    {
        int size;

        if(isNN) {
            line3 = "NN coding...";
            Interlocked.Exchange(ref requestNN, 0);
            BBoxInit();
        } else {
            line3 = "Photo coding...";
        }

        yield return StartCoroutine("CoroutineEncode", isNN);

        size = PreparePacket(isNN);

        if (!isNN) {

        }

        yield return StartCoroutine("CoroutineSending", size);

        if(isNN) {
            yield return StartCoroutine("CoroutineReceiving");
        }

        Interlocked.Exchange(ref processing, WAIT_FRAME);
    }

    IEnumerator CoroutineSending(int size)
    {
        bool isAsync;
        int sent;

        using (var send_sae = new SocketAsyncEventArgs()) {
            try {
                send_sae.Completed += transfer_callback;
                send_sae.SetBuffer(buffer, 0, size);
                socketIsBusy = 1;
                isAsync = socket.SendAsync(send_sae);
            }
            catch (Exception e) {
                Debug.Log($"Error in CoroutineSending: {e}.");
                yield break;
            }

            if (!isAsync) {
                while (socketIsBusy == 1) {
                    yield return new WaitForEndOfFrame();
                }
            }


            if (send_sae.SocketError != SocketError.Success) {
                Debug.Log("Socket::Send: error = " + send_sae.SocketError);
                yield break;
            }

            sent = send_sae.BytesTransferred;
        }

        if (sent < size) {
            //Debug.LogFormat("Error after sending [{0} > {1}]: \"{2}\"", size, sent, send_sae.SocketError);
        }
        inMemStream.Dispose();

        line3 = "Photo sent...";
    }

    IEnumerator CoroutineReceiving()
    {
        bool isAsync = false;
        try {
            socketIsBusy = 1;
            isAsync = socket.ReceiveAsync(recv_sae);
        }
        catch (Exception e) {
            Debug.Log($"Error in CoroutineReceiving: {e}.");
            yield break;
        }
        if (!isAsync) {
            while (socketIsBusy == 1) {
                yield return new WaitForEndOfFrame();
            }
        }
        if (recv_sae.SocketError != SocketError.Success) {
            Debug.Log("Socket::Send: error = " + recv_sae.SocketError);
            yield break;
        }
        line3 = "Recv response.";

        BBoxesHandler(recv_sae.BytesTransferred);

        ++photoIndex;
    }

    private void BBoxesHandler(int rl)
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
                Debug.Log("BBoxesHandler: " + e);
                return;
            }
            stx = breader.ReadInt32();
            nbbox = breader.ReadInt32();

            if (stx != STX) {
                Debug.Log(string.Format("wrong STX ({0,5} != {1,5}.", stx, STX));
            }
            else
            if (nbbox > 0) {
                line1 = "";
                float _x = 0, _y = 0, _w = 0, _h = 0, _o = 0, _p = 0; int _cindex = 0;
                for (int nb = 0; nb < nbbox; ++nb) {
                    float x = breader.ReadSingle(); //is the x coordinate of the bbox center
                    float y = breader.ReadSingle(); //is the y coordinate of the bbox center
                    float w = breader.ReadSingle(); //is the width of the bbox
                    float h = breader.ReadSingle(); //is the height of the bbox
                    float o = breader.ReadSingle();
                    float p = breader.ReadSingle();
                    int cindex = breader.ReadInt32();

                    if(p > _p) {
                        _x = x;
                        _y = y;
                        _w = w;
                        _h = h;
                        _p = p;
                        _cindex = cindex;
                    }

                    Debug.LogFormat("[{0,5}:{1}]:\tx={2,6:F4}, y={3,6:F4}, w={4,6:F4}, h={5,6:F4}, o={6,6:F4}, p={7,6:F4}, name#{8,2}=" + categories[cindex] + ".", photoIndex, nb, x, y, w, h, o, p, cindex);
                    line1 += categories[cindex] + ",";
                }

                BBoxCorrect(_w, _h, _x, _y, _p, _cindex);
            }
            else
            if (nbbox == 0) {
                line1 = "No bbox found.";
                Debug.Log("[{0:5}] No bbox found.");
            }
            else {
                line1 = "Error during inference.";
                Debug.LogFormat("[{0:5}]: error during inference.", photoIndex);
            }
        }
        catch {
            Debug.Log("BBoxesHandler error.");
        }
        finally {
            Interlocked.Exchange(ref tapped, 0);
            Destroy(photoBBox);
        }
    }

    private void BBoxCorrect(float w, float h, float x, float y, float p, int cindex)
    {
        var z = distance;

        var box = Instantiate(bbox, hitPosition, Quaternion.identity) as GameObject;

        var quat = cameraMain.transform.rotation;
        box.transform.forward = quat * Vector3.forward;
        box.transform.Rotate(-box.transform.eulerAngles.x, 0, -box.transform.eulerAngles.z); // makes bbox vertical

        var quad = box.transform.Find("BBoxQuad").gameObject;
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

        box.transform.Find("BBoxLabel").gameObject.GetComponent<TextMesh>().text = $"[{Math.Ceiling(p * 100f)}%]\n{categories[cindex]}";
    }

    private int PreparePacket(bool isNN)
    {
        Stream stream;
        int size;

        size = (int)inMemStream.Size - (isNN ? 54 : 0);

        stream = inMemStream.AsStreamForRead();
        stream.Seek(isNN ? 54 : 0, SeekOrigin.Begin);

        int read_size = stream.Read(buffer, OVERHEAD_SIZE, size);
        if (size != read_size) { throw new Exception(string.Format("Reading error: read {0} on {1}.", read_size, size)); }

        Array.Copy(BitConverter.GetBytes(STX),  0, buffer, 0, 4);
        Array.Copy(BitConverter.GetBytes(size), 0, buffer, 4, 4);
        Array.Copy(BitConverter.GetBytes(isNN ? 1 : 0), 0, buffer, 8, 4);

        return size + OVERHEAD_SIZE;
    }

    IEnumerator CoroutineStartSocket()
    {
        IPAddress ipa = IPAddress.Parse(ip);
        outEP = new IPEndPoint(ipa, port);
        recv_sae = new SocketAsyncEventArgs();
        recv_sae.Completed += transfer_callback;
        socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        Interlocked.Exchange(ref socketIsBusy, 0);

        bool isAsync = false;
        var connect_sae = new SocketAsyncEventArgs();
        try {
            Debug.Log($"trying to connect to {outEP}...");
            line1 = $"trying to connect to {outEP}...";

            Interlocked.Exchange(ref socketIsBusy, 1);
            connect_sae.Completed += connect_callback; ///socket_(s, e) => autoResetEvent.Set();
            connect_sae.RemoteEndPoint = outEP;
            isAsync = socket.ConnectAsync(connect_sae);
        }
        catch (SocketException e) {
            line1 = $"failed to connect to {outEP}!";
            Debug.Log($"failed: {e}\n");
        }


        int count = 0;
        while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
            Debug.Log($"waiting {count++}");
            if(count == 10) {
                DisposeSocket();
                break;
            }
            yield return new WaitForSeconds(1);
        }

        if (!socket.Connected) {
            DisposeSocket();
            line1 = $"connection failed: {connect_sae.SocketError}.";
        }
        else {

            socketStatus = SocketStatus.CONNECTED;
            Button1UpdateString($"STOP socket to\n{ip}:{port}");
            line1 = $"connected to {outEP}.";

            int r;
            byte[] config_buffer = new byte[16];
            try {
                line1 = "sending config...";

                Array.Copy(BitConverter.GetBytes((int)im_original_cols), 0, config_buffer, 0, 4);
                Array.Copy(BitConverter.GetBytes((int)im_original_rows), 0, config_buffer, 4, 4);
                Array.Copy(BitConverter.GetBytes((int)im_resized_cols), 0, config_buffer, 8, 4);
                Array.Copy(BitConverter.GetBytes((int)im_resized_rows), 0, config_buffer, 12, 4);

            }
            catch (Exception e) {
                Debug.Log($"Something happen.\n{e}");
                line1 = $"Error in config.";
            }

            yield return StartCoroutine(SendAsyncCoroutine(config_buffer, config_buffer.Length));

            Debug.Log("Sent config.");
            line1 = "config sent.";

            config_buffer[0] = 0;

            recv_sae.SetBuffer(config_buffer, 0, 12);

            yield return StartCoroutine("RecvCoroutine");

            int ln = 0, nn;
            if (BitConverter.ToInt32(config_buffer, 0) == STX) {
                ln = BitConverter.ToInt32(config_buffer, 4);
                nn = BitConverter.ToInt32(config_buffer, 8);
            }
            else {
                Debug.Log($"Wrong STX config buffer.");
            }
            Debug.Log($"Recv ln={ln} and nn={nn}.");
            line1 = $"Classes: l={ln}, n={nn}.";

            byte[] cbuf = new byte[ln];
            recv_sae.SetBuffer(cbuf, 0, ln);
            yield return StartCoroutine("RecvCoroutine");

            if (recv_sae.BytesTransferred < ln) {
                Debug.Log($"Too less bytes for class names.");
            }

            try {
                int ci = 0;
                BinaryReader br = new BinaryReader(new MemoryStream(cbuf));
                StringBuilder sb = new StringBuilder("");
                categories = new string[nn];
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
                        Debug.Log($"Error in recv classes while loop: {e}");
                    }
                }
            }
            catch (Exception e) {
                Debug.Log($"Error in recv classes: {e.Message}.");
            }

            try {
                buffer_size = OVERHEAD_SIZE + (uint)im_resized_size_bytes;
                buffer = new byte[buffer_size + 10];
                rbuffer = new byte[160];
                recv_sae.SetBuffer(rbuffer, 0, 150);
            }
            catch {
                Debug.Log("Error in RSetBuffer: 482.");
            }

            Array.Copy(STXb, buffer, 4);

            Debug.Log($"Buffer length: {buffer.Length}\n");

            line1 = $"Config received.";



            recognizer = new GestureRecognizer();
            recognizer.SetRecognizableGestures(GestureSettings.Tap);
            recognizer.Tapped += TapHandler;
            recognizer.StartCapturingGestures();


            enabled = true;
            processing = WAIT_FRAME;
        }
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
        line1 = e.SocketError.ToString();
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

    public IEnumerator SendAsyncCoroutine(byte[] b, int l)
    {
        bool isAsync = false;
        send_sae = new SocketAsyncEventArgs();
        try {
            send_sae.Completed += transfer_callback;
            try {
                send_sae.SetBuffer(b, 0, l);
            }
            catch {
                Debug.Log("Error in SSetBuffer: 505.");
            }
            Interlocked.Exchange(ref socketIsBusy, 1);
            isAsync = socket.SendAsync(send_sae);
        }
        catch (Exception e) {
            Debug.Log("Socket::Send:\t" + e.Message);
        }
        while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
            yield return null;
        }
        if (send_sae.SocketError != SocketError.Success) Debug.Log("Socket::Send: error = " + send_sae.SocketError);
        send_sae.Dispose();
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

    public IEnumerator RecvCoroutine()
    {
        bool isAsync = false;
        try {
            Interlocked.Exchange(ref socketIsBusy, 1);
            isAsync = socket.ReceiveAsync(recv_sae);
        }
        catch (Exception e) {
            Debug.Log(e.Message);
        }
        while (isAsync && 1 == Interlocked.CompareExchange(ref socketIsBusy, 0, 0)) {
            yield return null;
        }
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
        line2 = "starting capturer.";

        MediaFrameSource mediaFrameSource;
        var allGroups = await MediaFrameSourceGroup.FindAllAsync();
        if (allGroups.Count <= 0) {
            line2 = "no media!";
            return;
        }
        mediaCapture = new MediaCapture();
        var settings = new MediaCaptureInitializationSettings
        {
            SourceGroup = allGroups[0],
            SharingMode = MediaCaptureSharingMode.SharedReadOnly,
            StreamingCaptureMode = StreamingCaptureMode.Video,
            MemoryPreference = MediaCaptureMemoryPreference.Cpu
        };

        await mediaCapture.InitializeAsync(settings);

        string formats = "";

        mediaFrameSource = mediaCapture.FrameSources.Values.Single(x => x.Info.MediaStreamType == MediaStreamType.VideoRecord);
        try {
            MediaFrameFormat targetResFormat = null;
            foreach (var f in mediaFrameSource.SupportedFormats.OrderBy(x => x.VideoFormat.Width * x.VideoFormat.Height)) {
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
            return;
        }



        try {
            frameReader = await mediaCapture.CreateFrameReaderAsync(mediaFrameSource, MediaEncodingSubtypes.Bgra8);
            frameReader.AcquisitionMode = MediaFrameReaderAcquisitionMode.Realtime;
            frameReader.FrameArrived += OnFrameArrived;
        }
        catch {
            line2 = "no event handling!";
            return;
        }

        //	gr: taken from here https://forums.hololens.com/discussion/2009/mixedrealitycapture
        videoEffect = new VideoMRCSettings(true, 0.9f, true, 0);
        //if (mediaCapture.MediaCaptureSettings.VideoDeviceCharacteristic == VideoDeviceCharacteristic.AllStreamsIdentical ||
        //    mediaCapture.MediaCaptureSettings.VideoDeviceCharacteristic == VideoDeviceCharacteristic.PreviewRecordStreamsIdentical) {
        //    // This effect will modify both the preview and the record streams, because they are the same stream.
        //    videoEffectRecord =  await mediaCapture.AddVideoEffectAsync(videoEffect, MediaStreamType.VideoRecord);
        //}
        //else {
        videoEffectRecord =  await mediaCapture.AddVideoEffectAsync(videoEffect, MediaStreamType.VideoRecord);
            //videoEffectPreview = await mediaCapture.AddVideoEffectAsync(videoEffect, MediaStreamType.VideoPreview);
        //}

        await frameReader.StartAsync();

        line2 = "capturer started.";
    }

    private void PrepareEncoderNN()
    {
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
//	from https://forums.hololens.com/discussion/2009/mixedrealitycapture
public class VideoMRCSettings : IVideoEffectDefinition
{
    public string ActivatableClassId
    {
        get
        {
            return "Windows.Media.MixedRealityCapture.MixedRealityCaptureVideoEffect";
        }
    }

    public IPropertySet Properties
    {
        get; private set;
    }

    public VideoMRCSettings(bool HologramCompositionEnabled, float GlobalOpacityCoefficient, bool VideoStabilizationEnabled = false, int VideoStabilizationBufferLength = 0)
    {
        Properties = (IPropertySet)new PropertySet();
        Properties.Add("HologramCompositionEnabled", HologramCompositionEnabled);
        Properties.Add("VideoStabilizationEnabled", VideoStabilizationEnabled);
        Properties.Add("VideoStabilizationBufferLength", VideoStabilizationBufferLength);
        Properties.Add("GlobalOpacityCoefficient", GlobalOpacityCoefficient);
    }
}

public enum CTEnum
{
    NONE, CROPPED, SCALED, SQUARED
}
#endif

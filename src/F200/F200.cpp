#include "../Common.h"
#include "F200.h"

#ifndef WIN32

using namespace rs;

namespace f200
{
    
F200Camera::F200Camera(uvc_context_t * ctx, uvc_device_t * device, int idx) : UVCCamera(ctx, device, idx)
{
    
}

F200Camera::~F200Camera()
{
    
}

bool F200Camera::ConfigureStreams()
{
    if (streamingModeBitfield == 0)
        throw std::invalid_argument("No streams have been configured...");
    
    if (streamingModeBitfield & RS_STREAM_DEPTH)
    {
        StreamInterface * stream = new StreamInterface();
        stream->camera = this;
        
        if (!OpenStreamOnSubdevice(hardware, stream->uvcHandle, 1))
            throw std::runtime_error("Failed to open RS_STREAM_DEPTH (subdevice 1)");
        
        // Debugging
        uvc_print_stream_ctrl(&stream->ctrl, stdout);
        
        streamInterfaces.insert(std::pair<int, StreamInterface *>(RS_STREAM_DEPTH, stream));
    }
    
    if (streamingModeBitfield & RS_STREAM_RGB)
    {
        StreamInterface * stream = new StreamInterface();
        stream->camera = this;
        
        if (!OpenStreamOnSubdevice(hardware, stream->uvcHandle, 2))
            throw std::runtime_error("Failed to open RS_STREAM_RGB (subdevice 2)");
        
        // Debugging
        uvc_print_stream_ctrl(&stream->ctrl, stdout);
        
        streamInterfaces.insert(std::pair<int, StreamInterface *>(RS_STREAM_RGB, stream));
    }
    
    GetUSBInfo(hardware, usbInfo);
    std::cout << "Serial Number: " << usbInfo.serial << std::endl;
    std::cout << "USB VID: " << usbInfo.vid << std::endl;
    std::cout << "USB PID: " << usbInfo.pid << std::endl;
    
    ////////////////////////////////////////////////////////////////////////////
    
    hardware_io.reset(new IVCAMHardwareIO(internalContext));
    
    const auto od = hardware_io->GetOpticalData();
    const auto calib = hardware_io->GetParameters();
    
    rs_intrinsics rect = {},unrect = {};
    rs_extrinsics rotation ={{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f},{0.f,0.f,0.f}};
    
    rect.image_size[0] = 640;
    rect.image_size[1] = 480;
    rect.focal_length[0] =  od.IRUndistortedFocalLengthPxl.x;
    rect.focal_length[1] =  od.IRUndistortedFocalLengthPxl.y;
    rect.principal_point[0] =  od.IRPrincipalPoint.x + 320;
    rect.principal_point[1] =  od.IRPrincipalPoint.y + 240;
    
    unrect.image_size[0] = 640;
    unrect.image_size[1] = 480;
    unrect.focal_length[0] =  od.IRDistortedFocalLengthPxl.x;
    unrect.focal_length[1] =  od.IRDistortedFocalLengthPxl.y;
    unrect.principal_point[0] =  od.IRPrincipalPoint.x + 320;
    unrect.principal_point[1] =  od.IRPrincipalPoint.y + 240;
    unrect.distortion_coeff[0] = calib.Distc[0];
    unrect.distortion_coeff[1] = calib.Distc[1];
    unrect.distortion_coeff[2] = calib.Distc[2];
    unrect.distortion_coeff[3] = calib.Distc[3];
    unrect.distortion_coeff[4] = calib.Distc[4];

    calibUtils.reset(new CalibrationUtils(640,480,rect,unrect,rotation));
    
    ////////////////////////////////////////////////////////////////////////////
    
    return true;
}

void F200Camera::StartStream(int streamIdentifier, const StreamConfiguration & c)
{
    auto stream = streamInterfaces[streamIdentifier];
    
    if (stream->uvcHandle)
    {
        stream->fmt = static_cast<uvc_frame_format>(c.format);
        
        uvc_error_t status = uvc_get_stream_ctrl_format_size(stream->uvcHandle, &stream->ctrl, stream->fmt, c.width, c.height, c.fps);
        
        if (status < 0)
        {
            uvc_perror(status, "uvc_get_stream_ctrl_format_size");
            throw std::runtime_error("Open camera_handle Failed");
        }
        
        switch (c.format)
        {
            case FrameFormat::INVR:
            case FrameFormat::INVZ:
                depthFrame.reset(new TripleBufferedFrame(c.width, c.height, 2)); break;
            case FrameFormat::YUYV:
                colorFrame.reset(new TripleBufferedFrame(c.width, c.height, 3)); break;
            default:
                throw std::runtime_error("invalid frame format");
        }
        
        uvc_error_t startStreamResult = uvc_start_streaming(stream->uvcHandle, &stream->ctrl, &UVCCamera::cb, stream, 0);
        
        if (startStreamResult < 0)
        {
            uvc_perror(startStreamResult, "start_stream");
            throw std::runtime_error("Could not start stream");
        }

    }
}

void F200Camera::StopStream(int streamNum)
{
    //@tofix
}
    
rs_intrinsics F200Camera::GetStreamIntrinsics(int stream)
{
    // After having configured streams... optical data
    
    const CameraCalibrationParameters & ivCamParams = hardware_io->GetParameters();
    // undistorted for rgb
    return {{640,480},{500,500},{320,240},{1,0,0,0,0}}; // TODO: Use actual calibration data
}

rs_extrinsics F200Camera::GetStreamExtrinsics(int from, int to)
{
    return {{1,0,0,0,1,0,0,0,1},{0,0,0}};
}

} // end f200

#endif

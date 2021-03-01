// 参考资料： https://blog.csdn.net/leixiaohua1020/article/details/38868499

#include <iostream>
#include <string>
#include <map>
#include <SDL2/SDL.h>
#include <stdio.h>

using std::cout;
using std::endl;

#define MYTIMER_FLUSH_EVT   (SDL_USEREVENT + 1)

static bool thread_pause = false;
static bool thread_exit = false;
static std::map<std::string, Uint32> format_map = {
        {"yu12", SDL_PIXELFORMAT_IYUV},
        {"iyuv", SDL_PIXELFORMAT_IYUV},    //alias yu12
        {"yuv420p", SDL_PIXELFORMAT_IYUV}, //alias yu12
        {"yv12", SDL_PIXELFORMAT_YV12},
        {"nv12", SDL_PIXELFORMAT_NV12},
        {"nv21", SDL_PIXELFORMAT_NV21}
};

static int mytimer(void *data)
{
    (void)data;

    while (!thread_exit) {
        if (!thread_pause) {
            SDL_Event event;
            event.type = MYTIMER_FLUSH_EVT;
            SDL_PushEvent(&event); // 触发事件
        }
        SDL_Delay(40); // 延时40ms，即按照25fps帧速率播放视频
    }

    return 0;
}

static void usage(void)
{
    cout << "Usage:   YuvPlayer Pixelformat Size Yuvfile" << endl;
    cout << "Example: YuvPlayer yu12 1280x720 test.yuv" << endl;
    cout << "Note:    Pixelformat suport yu12, yv12, nv12, nv21 " << endl;
}

int main(int argc, char *argv[])
{
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    SDL_Thread *tid;
    int width, height;
    int screen_width, screen_height;
    Uint32 format;
    bool player_end = false;

    // 0. 解析输入参数
    const char *str_pixelformat = argv[1];
    const char *str_size = argv[2];
    const char *str_filename = argv[3];
    if (argc != 4) {
        usage();
        return -1;
    }
    if (format_map.find(str_pixelformat) == format_map.end()) {
        cout << "Pixelformat error!!!" << endl;
        usage();
        return -1;
    }
    format = format_map[str_pixelformat];
    if (sscanf(str_size, "%dx%d", &width, &height) != 2) {
        cout << "Size error!!!" << endl;
        usage();
        return -1;
    }
    if (width <= 0 || height <= 0) {
        cout << "Size error!!!" << endl;
        usage();
        return -1;
    }
    cout << "pixelformat:" << format << "  widht:" << width << "  height:" << height << endl;

    // 1. 打开yuv文件
    FILE *f = fopen(str_filename, "rb");
    if (!f) {
        cout << "fopen failed." << endl;
        return -1;
    }
    screen_width = width;
    screen_height = height;
    int frame_size = width * height * 3/2;
    uint8_t *frame = new uint8_t[frame_size];

    // 2. 初始化SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);
    window = SDL_CreateWindow("YuvPlayer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_width, screen_height, SDL_WINDOW_SHOWN);
    // 如果要将视频播放嵌入式到MFC/QT等的窗口中，可参考使用SDL_CreateWindowFrom接口
    if (!window) {
        cout << "SDL_CreateWindow faild: " << SDL_GetError() << endl;
        goto OUT;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        cout << "SDL_CreateRenderer faild: " << SDL_GetError() << endl;
        goto OUT;
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);
    // 像素格式对应关系：
    //        YU12(YUV420P/IYUV)  ->       SDL_PIXELFORMAT_IYUV
    //        YV12                ->       SDL_PIXELFORMAT_YV12
    //        NV12                ->       SDL_PIXELFORMAT_NV12
    //        NV12                ->       SDL_PIXELFORMAT_NV21
    // 纹理的宽高可以和window不一样，可以是window上的一个区域. 换句话说，一个window上可以画多个纹理
    if (!texture) {
        cout << "SDL_CreateTexture faild: " << SDL_GetError() << endl;
        goto OUT;
    }

    // 3. 初始化窗口界面为黑色
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // 设置画笔颜色
    SDL_RenderClear(renderer);                    // 使用画笔颜色清空渲染器
    SDL_RenderPresent(renderer);                  // 执行渲染

    // 4. 创建定时线程
    tid = SDL_CreateThread(mytimer, "mytimer", NULL);

    // 5. 循环处理事件
    while (1) {
        SDL_Event event;
        int ret = SDL_WaitEvent(&event); //阻塞等待事件
        if (!ret) {
            cout << "wait error" << endl;
            break;
        }

        if (event.type == SDL_QUIT) { // 关闭事件，点击窗口上的关闭图标可以退出
            cout << "got event: QUIT" << endl;
            break;
        }
        else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) { // 空格键实现播放暂停
            cout << "got event: KEY SAPCE" << endl;
            thread_pause = !thread_pause;
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN) { // 点击鼠标事件
            cout << "got event: MOUSE BUTTON (" << event.button.x << "," << event.button.y << ")" << endl;
        }
        else if (event.type == MYTIMER_FLUSH_EVT) { // 自定义事件
            if (player_end)
                continue;
            if (fread(frame, frame_size, 1, f) <= 0) {
                cout << "read file end." << endl;
                player_end = true;  // 播放完了不自动退出，而是显示最后一帧，等待用户点击关闭
            }
            // 将YUV更新到纹理上
            // 这里没有填充，pitch(即stride)和width相等
            // 注意pixels数据的Y/U/V应为连续内存区域，否则可改用 SDL_UpdateYUVTexture 分别指定Y/U/V的plane和pitch
            // SDL_UpdateYUVTexture仅支持YV12 or IYUV两种像素格式.
            SDL_UpdateTexture(texture, NULL, frame, width);

            SDL_Rect rect = {0, 0, width, height};
            SDL_RenderCopy(renderer, texture, NULL, &rect);// 将纹理拷贝到渲染器 (这里srcrect和dstrect可以指定区域，如果为NULL则为全部区域；
                                                           // 如果指定全部区域，而两个区域大小不同，则会自动对图像进行缩放)
            SDL_RenderPresent(renderer);                   // 执行渲染
        }
    }

    // 6. 等待线程退出，并回收线程资源
    thread_exit = 1;
    SDL_WaitThread(tid, NULL);

OUT:
    if (texture)
        SDL_DestroyTexture(texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
    delete[] frame;
    fclose(f);
    return 0;
}

// 总结：
//   1. 目前测试发现视频的渲染必须放在主线程中，在子线程中渲染图像出不来；
// 
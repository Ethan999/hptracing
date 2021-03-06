/* 
* @Author: BlahGeek
* @Date:   2015-01-18
* @Last Modified by:   BlahGeek
* @Last Modified time: 2015-12-29
*/

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <iostream>
#include <fstream>
#include <string>
#include "hp/common.h"
#include "hp/trace_runner.h"
#include "hp/scene/kdtree.h"

#include <Eigen/Dense>
#define cimg_display 0
#include "./CImg/CImg.h"

#include "OptionParser/OptionParser.h"

using namespace std;
using namespace hp;
using optparse::OptionParser;

static cl_float3 str2float3(std::string s) {
    cl_float3 ret;
    for(int i = 0 ; i < 3 ; i += 1) {
        ret.s[i] = atof(s.c_str());
        if(i < 2)
            s = s.substr(s.find(',')+1);
    }
    return ret;
}

// runner
std::unique_ptr<TraceRunner> runner = nullptr;
// options
cl_float3 view_point;
cl_float3 up_dir, right_dir;
int width, height;
float angle;
bool supersample;
cl_float3 background_color;
int sample, depth;
bool no_diffuse;
float brightness;
// results
unsigned char * pixels = nullptr;
int pixels_size = 0;

bool need_rerun = true;
float translate_step = 1.f;

static void runit() {
    if(!runner) {
        hp_log("WARNING: Runner is not ready");
        return;
    }
    if(pixels == nullptr || pixels_size != width * height * 4) {
        if(pixels) delete [] pixels;
        pixels_size = width * height * 4;
        pixels = new unsigned char [pixels_size];
    }
    hp_log("Rendering image... %dx%d, %d samples, max-depth %d",
           width, height, sample, depth);
    TickTock timer;
    try{
        runner->run(pixels, view_point, up_dir, right_dir,
                    float(width) / float(height) * angle, angle,
                    width, height, supersample?2:1, supersample?2:1,
                    background_color,
                    sample, depth, no_diffuse?1:0, brightness);
    } catch(cl::Error & err) {
        hp_log("Catch error: %d: %s", err.err(), err.what());
        hp_assert(false);
    }
    timer.timeit("Render done");
}

static void printString(std::string s, int y = 280, int x = 15) {
    glRasterPos2i(x, y);
    for(auto & c: s)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    hp_log(s.c_str());
}

static void displayFunc() {
    if(pixels == nullptr) return;
    glClear(GL_COLOR_BUFFER_BIT);
    glRasterPos2i(0, 0);
    glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.f, 0.f, 0.5f, 0.2f);
    glRecti(0, 0, 180, 210);

    glColor3f(1.f, 1.f, 1.f);
    printString("hpTracing by BlahGeek", 195);
    printString(ssprintf(" --view=%.2f,%.2f,%.2f", 
                view_point.s[0], view_point.s[1], view_point.s[2]), 170);
    printString(ssprintf("   --up=%.2f,%.2f,%.2f", 
                up_dir.s[0], up_dir.s[1], up_dir.s[2]), 155);
    printString(ssprintf("--right=%.2f,%.2f,%.2f", 
                right_dir.s[0], right_dir.s[1], right_dir.s[2]), 140);
    printString(ssprintf("%dx%d, %d samples", width, height, sample), 115);
    printString(ssprintf("Translate step: %.2f", translate_step), 100);
    printString(ssprintf("Max depth: %d, Angle: %.2f", depth, angle), 85);
    printString(ssprintf("No diffuse: %d", no_diffuse), 60);
    printString(ssprintf("Super-samp: %d", supersample), 45);
    printString(ssprintf("Brightness: %.2f", brightness), 30);

    glDisable(GL_BLEND);

    glutSwapBuffers();
}

static void timerFunc(int _) {
    glutTimerFunc(10, timerFunc, 0);
    if(!need_rerun) return;
    runit();
    need_rerun = false;
    glutPostRedisplay();
}

static void rotateView(float alpha, int ax) {
    Vec v_up_dir, v_right_dir, v_front_dir;
    ASSIGN_V3(v_up_dir, up_dir);ASSIGN_V3(v_right_dir, right_dir);
    v_front_dir = v_up_dir.cross(v_right_dir);

    Vec * about = &v_right_dir;
    if(ax == 2) about = &v_up_dir;
    if(ax == 3) about = &v_front_dir;

    Eigen::Matrix<float, 3, 3> L;
    L << 0, (*about)[2], -(*about)[1],
         -(*about)[2], 0, (*about)[0],
         (*about)[1], -(*about)[0], 0;
    Eigen::Matrix<float, 3, 3> m = Eigen::Matrix<float, 3, 3>::Identity();
    m += std::sin(alpha / 180.0 * PI) * L;
    m += (1.f - std::cos(alpha / 180.0 * PI)) * (L * L);

    if(&v_up_dir != about) v_up_dir = m * v_up_dir;
    if(&v_right_dir != about) v_right_dir = m * v_right_dir;
    if(&v_front_dir != about) v_front_dir = m * v_front_dir;

    ASSIGN_F3(up_dir, v_up_dir);
    ASSIGN_F3(right_dir, v_right_dir);
}

static void translateView(float translate, int ax) {
    Vec v_up_dir, v_right_dir, v_front_dir;
    ASSIGN_V3(v_up_dir, up_dir);ASSIGN_V3(v_right_dir, right_dir);
    v_front_dir = v_up_dir.cross(v_right_dir);

    Vec * about = &v_right_dir;
    if(ax == 2) about = &v_up_dir;
    if(ax == 3) about = &v_front_dir;

    Vec v_view_point; ASSIGN_V3(v_view_point, view_point);
    v_view_point += translate * (*about);

    ASSIGN_F3(view_point, v_view_point);
}

static void keyFunc(unsigned char key, int x, int y) {
    switch(key) {
        case 27: // ESC
            hp_log("ESC pressed, exit");
            exit(0);
            break;
        case 'z':
            if(angle > 0.1) angle -= 0.1;
            need_rerun = true;
            break;
        case 'x':
            angle += 0.1;
            need_rerun = true;
            break;
        case 'a': rotateView(-15, 2); need_rerun = true; break;
        case 'd': rotateView(15, 2); need_rerun = true; break;
        case 'w': rotateView(-15, 1); need_rerun = true; break;
        case 's': rotateView(15, 1); need_rerun = true; break;
        case 'q': rotateView(-15, 3); need_rerun = true; break;
        case 'e': rotateView(15, 3); need_rerun = true; break;

        case 'k': translateView(translate_step, 3); need_rerun = true; break;
        case 'j': translateView(-translate_step, 3); need_rerun = true; break;

        case '[': translate_step /= 2.f; break;
        case ']': translate_step *= 2.f; break;

        case '+': angle += 0.1f; need_rerun = true; break;
        case '-': angle -= 0.1f; need_rerun = true; break;
    };
}

static void specKeyFunc(int key, int x, int y) {
    switch(key) {
        case GLUT_KEY_UP: translateView(translate_step, 2); need_rerun = true; break;
        case GLUT_KEY_DOWN: translateView(-translate_step, 2); need_rerun = true; break;
        case GLUT_KEY_RIGHT: translateView(translate_step, 1); need_rerun = true; break;
        case GLUT_KEY_LEFT: translateView(-translate_step, 1); need_rerun = true; break;
    }
};

int main(int argc, char **argv) {
    OptionParser parser = OptionParser().description("HPTracing 0.0.1");
    parser.add_option("-i", "--input").dest("input");
    parser.add_option("--mtl").dest("mtl_basepath").set_default("");
    parser.add_option("--width").dest("width").type("int").set_default(500);
    parser.add_option("--height").dest("height").type("int").set_default(500);
    parser.add_option("--view").dest("view").set_default("0,0,-500");
    parser.add_option("--up").dest("up").set_default("0,1,0");
    parser.add_option("--right").dest("right").set_default("1,0,0");
    parser.add_option("--angle").dest("angle").type("float").set_default("1.0");
    parser.add_option("-x", "--supersample").dest("supersample").action("store_true").set_default("0");
    parser.add_option("-s", "--sample").dest("sample").type("int").set_default(10);
    parser.add_option("-d", "--depth").dest("depth").type("int").set_default(6);
    parser.add_option("--brightness").dest("brightness").type("float").set_default(1.0);
    parser.add_option("--no-diffuse").dest("no-diffuse").action("store_true").set_default("0");
    parser.add_option("--background").dest("background").set_default("0,0,0");
    parser.add_option("-o", "--output").dest("output").set_default("");

    optparse::Values options = parser.parse_args(argc, argv);
    vector<string> args = parser.args();

    TickTock timer;

    std::string input_path = options["input"];
    std::string mtl_basepath = options["mtl_basepath"];
    if(mtl_basepath.length() == 0) {
        auto pos = input_path.rfind('/');
        if(pos > 0) {
            mtl_basepath = input_path.substr(0, pos + 1);
            hp_log("MTL basepath: %s", mtl_basepath.c_str());
        }
    }

    auto scene = std::make_unique<KDTree>(options["input"], mtl_basepath);
    timer.timeit("Build KDTree done.");
    try{
        runner = std::make_unique<TraceRunner>(scene);
    } catch(cl::Error & err) {
        hp_log("Catch error: %d: %s", err.err(), err.what());
        hp_assert(false);
    }
    timer.timeit("Init OpenCL hardware & memories done");

    view_point = str2float3(options["view"]);
    up_dir = str2float3(options["up"]);
    right_dir = str2float3(options["right"]);
    width = (int)options.get("width");
    height = (int)options.get("height");
    angle = (float)options.get("angle");
    supersample = options.get("supersample");
    background_color = str2float3(options["background"]);
    sample = (int)options.get("sample");
    depth = (int)options.get("depth");
    no_diffuse = (bool)options.get("no-diffuse");
    brightness = (float)options.get("brightness");

    if(options["output"].length() == 0) {
        // init GLUT
        glutInitWindowSize(width, height);
        glutInitWindowPosition(0,0);
        glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
        glutInit(&argc, argv);
        glutCreateWindow("hpTracing by BlahGeek");
        glutDisplayFunc(displayFunc);
        glutTimerFunc(10, timerFunc, 0);
        // glutIdleFunc(idleFunc);
        glutKeyboardFunc(keyFunc);
        glutSpecialFunc(specKeyFunc);
        glViewport(0, 0, width, height);
        glLoadIdentity();
        glOrtho(0.f, width - 1.f, 0.f, height - 1.f, -1.f, 1.f);

        glutMainLoop();
    }
    else {
        // write to file
        auto image = cimg_library::CImg<unsigned char>(width, height, 1, 3);
        runit();
        for(int y = 0 ; y < height ; y += 1)
            for(int x = 0 ; x < width ; x += 1)
                for(int k = 0 ; k < 3 ; k += 1)
                    image(x, y, k) = pixels[(y * width + x) * 4 + k];
        image.mirror('y').save(options["output"].c_str());
    }
    return 0;
}


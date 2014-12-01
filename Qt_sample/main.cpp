//Yannick Verdie 2010

//--- Please read help() below: ---

#include <iostream>
#include <vector>

#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/legacy/compat.hpp>

#include <QtGui/QGuiApplication>
#include <QtGui/QMatrix4x4>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QScreen>

#include <QtCore/qmath.h>

#include "openglwindow.h"

/*
#if defined WIN32 || defined _WIN32 || defined WINCE
    #include <windows.h>
    #undef small
    #undef min
    #undef max
    #undef abs
#endif

#ifdef __APPLE__
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif
*/

class TriangleWindow : public OpenGLWindow
{
public:
	TriangleWindow();

	void initialize() Q_DECL_OVERRIDE;
	void render() Q_DECL_OVERRIDE;

private:
	GLuint loadShader(GLenum type, const char *source);

	GLuint m_posAttr;
	GLuint m_colAttr;
	GLuint m_matrixUniform;

	QOpenGLShaderProgram *m_program;
	int m_frame;
};

TriangleWindow::TriangleWindow()
	: m_program(0)
	, m_frame(0)
{
}

using namespace std;
using namespace cv;

static void help()
{
    cout << "\nThis demo demonstrates the use of the Qt enhanced version of the highgui GUI interface\n"
            "  and dang if it doesn't throw in the use of of the POSIT 3D tracking algorithm too\n"
            "It works off of the video: cube4.avi\n"
            "Using OpenCV version %s\n" << CV_VERSION << "\n\n"
" 1). This demo is mainly based on work from Javier Barandiaran Martirena\n"
"     See this page http://code.opencv.org/projects/opencv/wiki/Posit.\n"
" 2). This is a demo to illustrate how to use **OpenGL Callback**.\n"
" 3). You need Qt binding to compile this sample with OpenGL support enabled.\n"
" 4). The features' detection is very basic and could highly be improved \n"
"     (basic thresholding tuned for the specific video) but 2).\n"
" 5) THANKS TO Google Summer of Code 2010 for supporting this work!\n" << endl;
}

#define FOCAL_LENGTH 600
#define CUBE_SIZE 10
float OpenGLMatrix[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static void renderCube(float size)
{
    glBegin(GL_QUADS);
    // Front Face
    glNormal3f( 0.0f, 0.0f, 1.0f);
    glVertex3f( 0.0f,  0.0f,  0.0f);
    glVertex3f( size,  0.0f,  0.0f);
    glVertex3f( size,  size,  0.0f);
    glVertex3f( 0.0f,  size,  0.0f);
    // Back Face
    glNormal3f( 0.0f, 0.0f,-1.0f);
    glVertex3f( 0.0f,  0.0f, size);
    glVertex3f( 0.0f,  size, size);
    glVertex3f( size,  size, size);
    glVertex3f( size,  0.0f, size);
    // Top Face
    glNormal3f( 0.0f, 1.0f, 0.0f);
    glVertex3f( 0.0f,  size,  0.0f);
    glVertex3f( size,  size,  0.0f);
    glVertex3f( size,  size, size);
    glVertex3f( 0.0f,  size, size);
    // Bottom Face
    glNormal3f( 0.0f,-1.0f, 0.0f);
    glVertex3f( 0.0f,  0.0f,  0.0f);
    glVertex3f( 0.0f,  0.0f, size);
    glVertex3f( size,  0.0f, size);
    glVertex3f( size,  0.0f,  0.0f);
    // Right face
    glNormal3f( 1.0f, 0.0f, 0.0f);
    glVertex3f( size,  0.0f, 0.0f);
    glVertex3f( size,  0.0f, size);
    glVertex3f( size,  size, size);
    glVertex3f( size,  size, 0.0f);
    // Left Face
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f( 0.0f,  0.0f, 0.0f);
    glVertex3f( 0.0f,  size, 0.0f);
    glVertex3f( 0.0f,  size, size);
    glVertex3f( 0.0f,  0.0f, size);
    glEnd();
}


static void on_opengl(void* param)
{
    //Draw the object with the estimated pose
    glLoadIdentity();
    glScalef( 1.0f, 1.0f, -1.0f);
    glMultMatrixf( (float*)param );
    glEnable( GL_LIGHTING );
    glEnable( GL_LIGHT0 );
    glEnable( GL_BLEND );
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    renderCube( CUBE_SIZE );
    glDisable(GL_BLEND);
    glDisable( GL_LIGHTING );
}

static void initPOSIT(std::vector<CvPoint3D32f> *modelPoints)
{
    //Create the model pointss
    modelPoints->push_back(cvPoint3D32f(0.0f, 0.0f, 0.0f)); //The first must be (0,0,0)
    modelPoints->push_back(cvPoint3D32f(0.0f, 0.0f, CUBE_SIZE));
    modelPoints->push_back(cvPoint3D32f(CUBE_SIZE, 0.0f, 0.0f));
    modelPoints->push_back(cvPoint3D32f(0.0f, CUBE_SIZE, 0.0f));
}

static void foundCorners(vector<CvPoint2D32f> *srcImagePoints, const Mat& source, Mat& grayImage)
{
    cvtColor(source, grayImage, COLOR_RGB2GRAY);
    GaussianBlur(grayImage, grayImage, Size(11,11), 0, 0);
    normalize(grayImage, grayImage, 0, 255, NORM_MINMAX);
    threshold(grayImage, grayImage, 26, 255, THRESH_BINARY_INV); //25

    Mat MgrayImage = grayImage;
    //For debug
    //MgrayImage = MgrayImage.clone();//deep copy
    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;
    findContours(MgrayImage, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

    Point p;
    vector<CvPoint2D32f> srcImagePoints_temp(4,cvPoint2D32f(0,0));

    if (contours.size() == srcImagePoints_temp.size())
    {

        for(size_t i = 0 ; i<contours.size(); i++ )
        {

            p.x = p.y = 0;

            for(size_t j = 0 ; j<contours[i].size(); j++ )
                p+=contours[i][j];

            srcImagePoints_temp.at(i)=cvPoint2D32f(float(p.x)/contours[i].size(),float(p.y)/contours[i].size());
        }

        //Need to keep the same order
        //> y = 0
        //> x = 1
        //< x = 2
        //< y = 3

        //get point 0;
        size_t index = 0;
        for(size_t i = 1 ; i<srcImagePoints_temp.size(); i++ )
        {
            if (srcImagePoints_temp.at(i).y > srcImagePoints_temp.at(index).y)
                index = i;
        }
        srcImagePoints->at(0) = srcImagePoints_temp.at(index);

        //get point 1;
        index = 0;
        for(size_t i = 1 ; i<srcImagePoints_temp.size(); i++ )
        {
            if (srcImagePoints_temp.at(i).x > srcImagePoints_temp.at(index).x)
                index = i;
        }
        srcImagePoints->at(1) = srcImagePoints_temp.at(index);

        //get point 2;
        index = 0;
        for(size_t i = 1 ; i<srcImagePoints_temp.size(); i++ )
        {
            if (srcImagePoints_temp.at(i).x < srcImagePoints_temp.at(index).x)
                index = i;
        }
        srcImagePoints->at(2) = srcImagePoints_temp.at(index);

        //get point 3;
        index = 0;
        for(size_t i = 1 ; i<srcImagePoints_temp.size(); i++ )
        {
            if (srcImagePoints_temp.at(i).y < srcImagePoints_temp.at(index).y)
                index = i;
        }
        srcImagePoints->at(3) = srcImagePoints_temp.at(index);

        Mat Msource = source;
        stringstream ss;
        for(size_t i = 0 ; i<srcImagePoints_temp.size(); i++ )
        {
            ss<<i;
            circle(Msource,srcImagePoints->at(i),5,Scalar(0,0,255));
            putText(Msource,ss.str(),srcImagePoints->at(i),FONT_HERSHEY_SIMPLEX,1,Scalar(0,0,255));
            ss.str("");

            //new coordinate system in the middle of the frame and reversed (camera coordinate system)
            srcImagePoints->at(i) = cvPoint2D32f(srcImagePoints_temp.at(i).x-source.cols/2,source.rows/2-srcImagePoints_temp.at(i).y);
        }
    }

}

static void createOpenGLMatrixFrom(float *posePOSIT,const CvMatr32f &rotationMatrix, const CvVect32f &translationVector)
{


    //coordinate system returned is relative to the first 3D input point
    for (int f=0; f<3; f++)
    {
        for (int c=0; c<3; c++)
        {
            posePOSIT[c*4+f] = rotationMatrix[f*3+c];	//transposed
        }
    }
    posePOSIT[3] = 0.0;
    posePOSIT[7] = 0.0;
    posePOSIT[11] = 0.0;
    posePOSIT[12] = translationVector[0];
    posePOSIT[13] = translationVector[1];
    posePOSIT[14] = translationVector[2];
    posePOSIT[15] = 1.0;
}

int main(int argc, char **argv)
{
    help();
    VideoCapture video("cube4.avi");
    CV_Assert(video.isOpened());

    Mat source, grayImage;

    video >> source;

    namedWindow("original", WINDOW_AUTOSIZE | CV_WINDOW_FREERATIO | WINDOW_OPENGL);
    namedWindow("POSIT", WINDOW_AUTOSIZE | CV_WINDOW_FREERATIO | WINDOW_OPENGL);
    //displayOverlay("POSIT", "We lost the 4 corners' detection quite often (the red circles disappear). This demo is only to illustrate how to use OpenGL callback.\n -- Press ESC to exit.", 10000);

    //float OpenGLMatrix[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    setOpenGlDrawCallback("POSIT",on_opengl,OpenGLMatrix);

    vector<CvPoint3D32f> modelPoints;
    initPOSIT(&modelPoints);

    //Create the POSIT object with the model points
    CvPOSITObject* positObject = cvCreatePOSITObject( &modelPoints[0], (int)modelPoints.size() );

    CvMatr32f rotation_matrix = new float[9];
    CvVect32f translation_vector = new float[3];
    CvTermCriteria criteria = cvTermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 100, 1.0e-4f);

    vector<CvPoint2D32f> srcImagePoints(4,cvPoint2D32f(0,0));

	video >> source;
	imshow("original", source);
	foundCorners(&srcImagePoints, source, grayImage);
	cvPOSIT(positObject, &srcImagePoints[0], FOCAL_LENGTH, criteria, rotation_matrix, translation_vector);
	createOpenGLMatrixFrom(OpenGLMatrix, rotation_matrix, translation_vector);
	imshow("POSIT", source);

	QGuiApplication app(argc, argv);
	QSurfaceFormat format;
	format.setSamples(16);
	TriangleWindow window;
	window.setFormat(format);
	window.resize(640, 480);
	window.show();
	window.setAnimating(true);
	return app.exec();

    while(waitKey(33) != 27)
    {
        video >> source;
        imshow("original",source);

        foundCorners(&srcImagePoints,source,grayImage);
        cvPOSIT( positObject, &srcImagePoints[0], FOCAL_LENGTH, criteria, rotation_matrix, translation_vector );
        createOpenGLMatrixFrom(OpenGLMatrix,rotation_matrix,translation_vector);

        imshow("POSIT",source);

        if (video.get(CV_CAP_PROP_POS_AVI_RATIO) > 0.99)
            video.set(CV_CAP_PROP_POS_AVI_RATIO, 0);
    }

    destroyAllWindows();
    cvReleasePOSITObject(&positObject);

    return 0;
}

static const char *vertexShaderSource =
"attribute highp vec4 posAttr;\n"
"attribute lowp vec4 colAttr;\n"
"varying lowp vec4 col;\n"
"uniform highp mat4 matrix;\n"
"void main() {\n"
"   col = colAttr;\n"
"   gl_Position = matrix * posAttr;\n"
"}\n";

static const char *fragmentShaderSource =
"varying lowp vec4 col;\n"
"void main() {\n"
"   gl_FragColor = col;\n"
"}\n";

GLuint TriangleWindow::loadShader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, 0);
	glCompileShader(shader);
	return shader;
}

void TriangleWindow::initialize()
{
	m_program = new QOpenGLShaderProgram(this);
	m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
	m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
	m_program->link();
	m_posAttr = m_program->attributeLocation("posAttr");
	m_colAttr = m_program->attributeLocation("colAttr");
	m_matrixUniform = m_program->uniformLocation("matrix");
}

void TriangleWindow::render()
{
	const qreal retinaScale = devicePixelRatio();
	glViewport(0, 0, width() * retinaScale, height() * retinaScale);

	glClear(GL_COLOR_BUFFER_BIT);
	/*
	m_program->bind();

	QMatrix4x4 matrix;
	matrix.perspective(60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
	matrix.translate(0, 0, -2);
	matrix.rotate(100.0f * m_frame / screen()->refreshRate(), 0, 1, 0);

	m_program->setUniformValue(m_matrixUniform, matrix);

	GLfloat vertices[] = {
		0.0f, 0.707f,
		-0.5f, -0.5f,
		0.5f, -0.5f
	};

	GLfloat colors[] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	};

	glVertexAttribPointer(m_posAttr, 2, GL_FLOAT, GL_FALSE, 0, vertices);
	glVertexAttribPointer(m_colAttr, 3, GL_FLOAT, GL_FALSE, 0, colors);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	*/

	//Draw the object with the estimated pose
	glLoadIdentity();
	glScalef(1.0f, 1.0f, -1.0f);
	glMultMatrixf((float*)OpenGLMatrix);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	renderCube(CUBE_SIZE);
	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	Mat test(cv::Size(4, 4), CV_32FC1, OpenGLMatrix);
	test = test.t();

	// NOTES:
	// Black screen.
	// - Delete all the shader code, maybe it interferes?
	// - Project a cube vertex manually with the given OpenGLMatrix
	// - Try to render it with the QMatrix4x4, .rotation(...) etc.

	//m_program->release();

	++m_frame;
}

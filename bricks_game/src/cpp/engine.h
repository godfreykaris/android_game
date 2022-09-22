
#include <deque>
#include <array>
#include <algorithm>
#include <chrono>

#include <GLES/gl.h>
#include <GLES2/gl2.h>

#include "vendor/OBJ_Loader.h"

#include "glm.hpp"
#include <gtc/matrix_transform.hpp>
#include <gtx/transform.hpp>

#include "logger.h"

#include "stb_image.h"

#define LOG_TAG "EglSample"

static int m_score = 0;
static bool m_paused = false;

static GLuint engine_width, engine_height;

GLfloat default_user_x_touchPoint; //half the height of the display

static GLfloat bar_position;

float angle = 0.0f;
static int x_down, y_down;


GLuint vShader;
GLuint fShader;

GLuint shader_program_id;

GLint compiled;
GLint infoLen;

GLuint set_up_and_compile_shader(GLenum shader_type, const std::string& shader_file)
{
	GLuint shader;
	//set up shader data
	{
		shader = glCreateShader(shader_type);
		
		const char* shader_data = shader_file.c_str();
		glShaderSource(shader, 1, &shader_data, NULL);
		
	}

	//shader compilation
	{
		glCompileShader(shader);
		
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);		
	}

	return shader;
}

GLuint load(const std::string& vertex_shader, const std::string& fragment_shader)
{
	//vertex shader
	vShader = set_up_and_compile_shader(GL_VERTEX_SHADER, vertex_shader);

	//fragment shader
	fShader = set_up_and_compile_shader(GL_FRAGMENT_SHADER, fragment_shader);

	//set up the shader program
	{
		shader_program_id = glCreateProgram();
		
		if (shader_program_id)
		{
			glAttachShader(shader_program_id, vShader);
			
			glAttachShader(shader_program_id, fShader);
			
			glLinkProgram(shader_program_id);
			
		}
	}

	return shader_program_id;
}

const char* mvshader =
{
	"attribute vec3 pos;\n"
	//"attribute vec3 norm;\n"
	"attribute vec2 tx;\n"

	"varying vec2 texCoords;\n"

	"uniform mat4 mvp;\n"

	"void main(void)\n"
	"{\n"
		"vec4 vp = vec4(pos, 1.0);\n"
		//"vec4 vn = vec4(norm, 1.0);\n"
		"vec4 md = mvp * vp ;\n"
		
		"gl_Position = md;\n"
		//"gl_Position = vp;\n"
		"texCoords = tx;\n"
	"}\n"
};

const char* mfshader =
{
	"precision mediump float;\n"

	"varying vec2 texCoords;\n"

	"uniform sampler2D texture_diffuse;\n"

	"void main()\n"
	"{\n"
		//"gl_FragColor = vec4(0.0,1.0, 0.0, 1.0);\n"
		"gl_FragColor = texture2D(texture_diffuse, texCoords);\n"
	"}\n"
};

const int total_renderable_bricks = 56;
const std::size_t total_brick_rows = 7, total_brick_columns = 8;

std::deque<bool> hit_list(total_renderable_bricks, true);

int max_hit_bricks = 25;
int total_hit_bricks = 0;

std::vector<int> bricks_to_be_rendered;
//get 20 unique random numbers such as (0 <= x <= 55); 
std::vector<int> get_brick_indices()
{
	std::vector<int> bricks_to_be_rendered;

	int64_t seed = time(0);
	srand(seed);

	std::vector<int>::iterator result;

	int random_no = 0;
	int count = 0;

	for (; count < max_hit_bricks;)
	{
		random_no = rand() % 56;
		//if the index is already added, get another unique one
		result = std::search_n(bricks_to_be_rendered.begin(), bricks_to_be_rendered.end(), 1, random_no);
		if (result == bricks_to_be_rendered.end())
		{
			bricks_to_be_rendered.push_back(random_no);
			count++;
		}
	}

	return bricks_to_be_rendered;
}
//mark the bricks that will be rendered
int mark_bricks(std::deque<bool>& hit_list)
{
	for (int i = 0; i < max_hit_bricks; i++)
	{
		hit_list[bricks_to_be_rendered[i]] = false;
	}

	return 0;
}

//reset the bricks hit list
int reset_hit_list(std::deque<bool>& hit_list, std::vector<int>& bricks_to_be_rendered, int& total_hit_bricks)
{
	std::deque<bool> default_list(total_renderable_bricks, true);
	hit_list = default_list;

	bricks_to_be_rendered = get_brick_indices();

	mark_bricks(hit_list);

	total_hit_bricks = 0;

	return 0;
}


//bricks coordinates in relation to a sphere coordinate(position)
std::array<float, total_brick_rows> possible_y_coords = { { -0.62, -2.42, -4.22, -6.02, -7.82, -9.62, -11.42 } };
std::array<float, total_brick_columns> possible_x_coords = { { 20.3, 15.03, 9.76, 4.49, -0.78, -6.05, -11.32, -16.59 } };

struct sphere_coordinates
{
	/*Default values are used for initialization*/
	GLfloat x = -5.0, y = 11.2;	
	// the x axis is rotated by glm::rotate so the right side is negative
	GLfloat left_wall = 21, right_wall = -17.6;

	GLfloat display_floor = 14.15, display_roof = -12.25;
	GLfloat bar_floor = 12.0;

	int32_t roof_floor_flag;
	int32_t walls_flag;
};

sphere_coordinates SphereCoords;


long elapsed_time = 0;

//check if brick had been hit, if not, add it to the hit bricks' record and tag it as hit
// brick center to: brick bottom or top = 1.82, brick left or right side = 2.0, side hit position of the ball = 3.0

// 1.82 is the difference between the center y coordinates and the top or bottom y coordinate
int current_brick_in_analyser = 0;
int test = 0;
int brick_analyser()
{
	current_brick_in_analyser = 0;  //initialize it to zero/first entry every time the function is called	
		
	for (int brick_row = 0; brick_row < total_brick_rows; brick_row++)
	{
		if (((SphereCoords.y) < (possible_y_coords[brick_row] + 1.82)) && ((SphereCoords.y) > (possible_y_coords[brick_row] - 1.82)))
		{
			for (int brick_column = 0; brick_column < total_brick_columns; brick_column++)
			{
				if ((SphereCoords.x < (possible_x_coords[brick_column] + 2.0)) && (SphereCoords.x > (possible_x_coords[brick_column] - 2.0)) && ((SphereCoords.y) > (possible_y_coords[brick_row])))
				{
					if (brick_row == 0)
					{
						if (brick_column == 0)
							current_brick_in_analyser = total_brick_rows;
						else
							current_brick_in_analyser = (total_brick_rows - brick_column);
					}
					else
					{
						if (brick_row == 1)
							current_brick_in_analyser = (total_brick_rows - brick_column) + total_brick_columns;
						else
							current_brick_in_analyser = (brick_row * total_brick_columns) + (total_brick_columns - (brick_column + 1));
					}
					
					if (hit_list[current_brick_in_analyser] != true)
					{
						hit_list[current_brick_in_analyser] = true;
						total_hit_bricks++;
						SphereCoords.roof_floor_flag = 1;						
						m_score++;
						break;
					}					
				}
				
				else if ((SphereCoords.x < (possible_x_coords[brick_column] + 2.0)) && (SphereCoords.x > (possible_x_coords[brick_column] - 2.0)) && ((SphereCoords.y) < (possible_y_coords[brick_row])))
				{
					if (brick_row == 0)
					{
						if (brick_column == 0)
							current_brick_in_analyser = total_brick_rows;
						else
							current_brick_in_analyser = (total_brick_rows - brick_column);
					}
					else
					{
						if (brick_row == 1)
							current_brick_in_analyser = (total_brick_rows - brick_column) + total_brick_columns;
						else
							current_brick_in_analyser = (brick_row * total_brick_columns) + (total_brick_columns - (brick_column + 1));
					}

					if (hit_list[current_brick_in_analyser] != true)
					{
						hit_list[current_brick_in_analyser] = true;
						total_hit_bricks++;
						SphereCoords.roof_floor_flag = 0;
						m_score++;						
						break;
					}
				}
				else if ((SphereCoords.x < (possible_x_coords[brick_column] + 3.1)) && (SphereCoords.x >(possible_x_coords[brick_column] - 3.1)))
				{
					if (brick_row == 0)
					{
						if (brick_column == 0)
							current_brick_in_analyser = 0;
						else
							current_brick_in_analyser = total_brick_rows - brick_column;
					}
					else
					{
						if (brick_row == 1)
							current_brick_in_analyser = (total_brick_rows - brick_column) + total_brick_columns;
						else
							current_brick_in_analyser = (brick_row * total_brick_columns) + (total_brick_rows - brick_column);
					}

					if (hit_list[current_brick_in_analyser] != true)
					{
						hit_list[current_brick_in_analyser] = true;
						total_hit_bricks++;
						m_score++;
						if (SphereCoords.x < possible_x_coords[brick_column])
								SphereCoords.walls_flag = 0;
						else if (SphereCoords.x > possible_x_coords[brick_column])
								SphereCoords.walls_flag = 1;
										
						break;
					}
					
				}

			}

		}
				
	}

	//elapsed_time = tm.duration() - 36150;
	return 0;
}

//distance from brick center to top or bottom of the brick i.e half the brick height = 1.82;
GLfloat brick_TopBottom_midpoint = 1.82;
//default x and y speeds
GLfloat x_axis_speed = 0.71 , y_axis_speed = 0.4035 * brick_TopBottom_midpoint;
GLfloat max_y_speed = 0.7401334 * brick_TopBottom_midpoint, max_x_speed = 1.328;
GLfloat min_y_speed = 0.0598666 * brick_TopBottom_midpoint, min_x_speed = 0.112;

void speed_controller(GLuint width, GLuint height, int32_t user_x_touchPoint, int32_t user_y_touchPoint)
{
	//Are we touching the speed controller?
	if (user_x_touchPoint > (width * 0.8895027) && user_x_touchPoint < (width) &&
			user_y_touchPoint >(height * 0.7037037) && user_y_touchPoint < (height))
	{
			//user_y_touchPoint > height * 0.85046295  implies that we are at the reduce speed section
			if (((user_y_touchPoint > height * 0.85046295) && (x_axis_speed < min_x_speed || y_axis_speed < min_y_speed)) || 
			    ((user_y_touchPoint < height * 0.85046295) && (x_axis_speed > max_x_speed || y_axis_speed > max_y_speed)))
				
			{/*we do nothing if the speed is minimum or maximum and the player is reducing or increasing it*/}
			
			else if (user_y_touchPoint > height * 0.85046295 && y_axis_speed > min_y_speed && x_axis_speed > min_x_speed)
			{
				y_axis_speed -= 0.0528666 * brick_TopBottom_midpoint;				
				x_axis_speed -= 0.092;
			}
			else if (y_axis_speed < max_y_speed && x_axis_speed < max_x_speed)
			{
				y_axis_speed += 0.0528666 * brick_TopBottom_midpoint;
				x_axis_speed += 0.092;
			}
	}
}


GLfloat sphere_x_coordinate = 0;
int32_t brick_sphere_x_coordinate = 0;
//The y coordinate is a float, we truncate it to a whole number. It is what we need.
int32_t sphere_y_coordinate = 0;

//calculate the sphere coordinates(direction)
sphere_coordinates sphere_coords(GLfloat bar_x_coord)
{	
	//check if we have hit a brick if we are at  the bricks location area (first row is < 0.92)
	if(SphereCoords.y < 0.92)
		brick_analyser();
	
	//convert sphere x axis to brick x axis
	brick_sphere_x_coordinate = SphereCoords.x * 0.7619 * 0.46736;	
	//convert sphere x axis to bar x axis
	sphere_x_coordinate = SphereCoords.x * 0.0207996;

	sphere_y_coordinate = SphereCoords.y;
	//bar width = |0.2704| i.e the difference btwn sphere coordinates(converted to bar coords) of both ends of the bar;
	//check if we are within bar width and also on the bar floor
	if ((sphere_x_coordinate < (bar_x_coord + 0.1352)) && (sphere_x_coordinate > (bar_x_coord - 0.1352)) && (sphere_y_coordinate == SphereCoords.bar_floor))
			SphereCoords.roof_floor_flag = 0;	
	//the offset is between 0.1 and 1
	if ((SphereCoords.y > (SphereCoords.display_floor - 0.1)) && (SphereCoords.y < (SphereCoords.display_floor + 3.5)))
			SphereCoords.roof_floor_flag = 0;
	else if ((SphereCoords.y > (SphereCoords.display_roof - 3.5)) && (SphereCoords.y < (SphereCoords.display_roof + 0.1)))
			SphereCoords.roof_floor_flag = 1;		
	
	if (SphereCoords.roof_floor_flag == 0)
		SphereCoords.y -= y_axis_speed;//SphereCoords.y -= 0.8 * 1.82;
	else if (SphereCoords.roof_floor_flag == 1)
		SphereCoords.y += y_axis_speed;//SphereCoords.y += 0.8 * 1.82;
		
	//the offset is between 0.1 and 1
	if ((SphereCoords.x > (SphereCoords.left_wall - 0.1)) && (SphereCoords.x < (SphereCoords.left_wall + 3.5)))
			SphereCoords.walls_flag = 0;	
	else if ((SphereCoords.x > (SphereCoords.right_wall - 3.5)) && (SphereCoords.x < (SphereCoords.right_wall + 0.1)))
			SphereCoords.walls_flag = 1;
	
	if (SphereCoords.walls_flag == 0)
			SphereCoords.x -= x_axis_speed;//SphereCoords.x -=  1.4;
	else if (SphereCoords.walls_flag == 1)
			SphereCoords.x += x_axis_speed;//SphereCoords.x +=  1.4;
	
	return SphereCoords;
}

GLfloat bar_x_position = 0.0;
GLfloat bar_displacement = 0.0;	                  //Displacement from the previous bar x coordinate

//bar distance from left wall ranging from 0.0 to 0.92, 0.92 is the difference between the 0.46(left wall) and -0.46(right wall)
GLfloat distance_from_leftWall = 0.0;
GLfloat barx_to_userx = default_user_x_touchPoint; //distance_from_leftWall converted to an x/horizontal screen touchPoint

GLfloat bar_x_pos(GLuint width, GLuint height, int user_x_touchPoint, int user_y_touchPoint)
{			
	//Is the touchpoint below( > than) the set bar user_y_touchPoint and to the left of(less than the start of) the menu area?
	if ((user_y_touchPoint > ((height) * 0.82407)) && (user_x_touchPoint < (width) * 0.8801105))  
	{
		if ((bar_x_position < 0) || (bar_x_position > 0))
			distance_from_leftWall = (0.46 - bar_x_position);	
		else if (bar_x_position == 0)
			distance_from_leftWall = 0.46;					    //midpoint of the walls
		barx_to_userx = (distance_from_leftWall / 0.92) * width;
		bar_displacement = (user_x_touchPoint - barx_to_userx)/-(width / 0.92);
		bar_x_position += bar_displacement;	
	}

	return bar_x_position;
}

glm::vec3 brickpositions[] = {
		glm::vec3(-1.0f, 0.0f,-4.7f),
		glm::vec3(-1.0f, 0.0f,-3.2f),
		glm::vec3(-1.0f, 0.0f,-1.7f),
		glm::vec3(-1.0f, 0.0f,-0.2f),
		glm::vec3(-1.0f, 0.0f, 1.3f),
		glm::vec3(-1.0f, 0.0f, 2.8f),
		glm::vec3(-1.0f, 0.0f, 4.3f),
		glm::vec3(-1.0f, 0.0f, 5.8f),

		glm::vec3(-4.0f, 0.0f,-4.7f),
		glm::vec3(-4.0f, 0.0f,-3.2f),
		glm::vec3(-4.0f, 0.0f,-1.7f),
		glm::vec3(-4.0f, 0.0f,-0.2f),
		glm::vec3(-4.0f, 0.0f, 1.3f),
		glm::vec3(-4.0f, 0.0f, 2.8f),
		glm::vec3(-4.0f, 0.0f, 4.3f),
		glm::vec3(-4.0f, 0.0f, 5.8f),

		
		glm::vec3(-7.0f, 0.0f,-4.7f),
		glm::vec3(-7.0f, 0.0f,-3.2f),
		glm::vec3(-7.0f, 0.0f,-1.7f),
		glm::vec3(-7.0f, 0.0f,-0.2f),
		glm::vec3(-7.0f, 0.0f, 1.3f),
		glm::vec3(-7.0f, 0.0f, 2.8f),
		glm::vec3(-7.0f, 0.0f, 4.3f),
		glm::vec3(-7.0f, 0.0f, 5.8f),

		glm::vec3(-10.0f, 0.0f,-4.7f),
		glm::vec3(-10.0f, 0.0f,-3.2f),
		glm::vec3(-10.0f, 0.0f,-1.7f),
		glm::vec3(-10.0f, 0.0f,-0.2f),
		glm::vec3(-10.0f, 0.0f, 1.3f),
		glm::vec3(-10.0f, 0.0f, 2.8f),
		glm::vec3(-10.0f, 0.0f, 4.3f),
		glm::vec3(-10.0f, 0.0f, 5.8f),

		glm::vec3(-13.0f, 0.0f,-4.7f),
		glm::vec3(-13.0f, 0.0f,-3.2f),
		glm::vec3(-13.0f, 0.0f,-1.7f),
		glm::vec3(-13.0f, 0.0f,-0.2f),
		glm::vec3(-13.0f, 0.0f, 1.3f),
		glm::vec3(-13.0f, 0.0f, 2.8f),
		glm::vec3(-13.0f, 0.0f, 4.3f),
		glm::vec3(-13.0f, 0.0f, 5.8f),

		glm::vec3(-16.0f, 0.0f,-4.7f),
		glm::vec3(-16.0f, 0.0f,-3.2f),
		glm::vec3(-16.0f, 0.0f,-1.7f),
		glm::vec3(-16.0f, 0.0f,-0.2f),
		glm::vec3(-16.0f, 0.0f, 1.3f),
		glm::vec3(-16.0f, 0.0f, 2.8f),
		glm::vec3(-16.0f, 0.0f, 4.3f),
		glm::vec3(-16.0f, 0.0f, 5.8f),

		glm::vec3(-19.0f, 0.0f,-4.7f),
		glm::vec3(-19.0f, 0.0f,-3.2f),
		glm::vec3(-19.0f, 0.0f,-1.7f),
		glm::vec3(-19.0f, 0.0f,-0.2f),
		glm::vec3(-19.0f, 0.0f, 1.3f),
		glm::vec3(-19.0f, 0.0f, 2.8f),
		glm::vec3(-19.0f, 0.0f, 4.3f),
		glm::vec3(-19.0f, 0.0f, 5.8f),

				
};

struct model_vertex
{
	glm::vec3 mdata;

};

struct txmodel_vertex
{
	GLfloat x, y;

};


struct my_model_vertices
{
	std::vector<model_vertex> positions;
	std::vector<model_vertex> normals;
	std::vector<txmodel_vertex> texture_coords;
};


//brick
objl::Loader mbrick;
my_model_vertices brick_vertices;
GLuint brick_texture;

//bar
objl::Loader mbar;
my_model_vertices bar_vertices;
GLuint bar_texture;

//sphere
objl::Loader msphere;
my_model_vertices sphere_vertices;
GLuint sphere_texture;

//speed
objl::Loader mspeed;
my_model_vertices speed_vertices;
GLuint speed_texture;

my_model_vertices load_3d_model(objl::Loader ld)
{
	my_model_vertices modelvertices;	
	
	int vertices_size = ld.LoadedVertices.size();
	for (int vertex_no = 0; vertex_no < vertices_size; vertex_no++)
	{		
		//Position
		model_vertex pos_vertices;				
		glm::vec3 pos_3d;
		
		pos_3d.x = ld.LoadedVertices[vertex_no].Position.X;
		pos_3d.y = ld.LoadedVertices[vertex_no].Position.Y;
		pos_3d.z = ld.LoadedVertices[vertex_no].Position.Z;
		pos_vertices.mdata = pos_3d;
		modelvertices.positions.push_back(pos_vertices);

		//Normals
		model_vertex normals_3d;
		glm::vec3 norm_3d;

		norm_3d.x = ld.LoadedVertices[vertex_no].Normal.X;
		norm_3d.y = ld.LoadedVertices[vertex_no].Normal.Y;
		norm_3d.z = ld.LoadedVertices[vertex_no].Normal.Z;
		normals_3d.mdata = norm_3d;
		modelvertices.normals.push_back(normals_3d);

		//Texture Coordinates
		txmodel_vertex tex_coords_3d;
		
		tex_coords_3d.x = ld.LoadedVertices[vertex_no].TextureCoordinate.X;
		tex_coords_3d.y = ld.LoadedVertices[vertex_no].TextureCoordinate.Y;

		modelvertices.texture_coords.push_back(tex_coords_3d);
		
	}
	
	return modelvertices;
}

GLuint load_3d_model_texture(unsigned char* image_data, GLint width, GLint height, const std::string& image_type)
{
	GLuint texture_id;
	glGenTextures(1, &texture_id);																				
	glBindTexture(GL_TEXTURE_2D, texture_id);																	

	//set the texture wrapping & filtering options (on the currently bound texture object)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);										
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);										
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);											
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);											

	if (image_data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);		
		glGenerateMipmap(GL_TEXTURE_2D);																		
		LOGDEBUG("Texture loaded successfully.");
	}
	else
		LOGDEBUG("Failed to load texture");
	
	glBindTexture(GL_TEXTURE_2D, 0);																			

	return texture_id;
}

void Bind_Texture(GLuint texture_target, GLuint texture_id, GLenum TextureUnit)
{
	glActiveTexture(TextureUnit);
	glBindTexture(texture_target, texture_id);
}

void Setup_VertexAttribPointers(const void* positions, const void* textureCoordinates)
{
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, positions);								
	glEnableVertexAttribArray(0);																
																								
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, textureCoordinates);						
	glEnableVertexAttribArray(1);
}

unsigned char* brick_image_data;
GLint brick_widt, brick_heigh;

unsigned char* bar_image_data;
GLint bar_widt, bar_heigh;

unsigned char* sphere_image_data;
GLint sphere_widt, sphere_heigh;

unsigned char* speed_image_data;
GLint speed_widt, speed_heigh;

static int data_retrieved_flag = 0;
void retrieve_models_data()
{	
	//brick
	brick_vertices = load_3d_model(mbrick);
	brick_texture = load_3d_model_texture(brick_image_data, brick_widt, brick_heigh, "jpg");
	//bar
	bar_vertices = load_3d_model(mbar);
	bar_texture = load_3d_model_texture(bar_image_data, bar_widt, bar_heigh, "png");
	//sphere
	sphere_vertices = load_3d_model(msphere);
	sphere_texture = load_3d_model_texture(sphere_image_data, sphere_widt, sphere_heigh, "png");
	//speed
	speed_vertices = load_3d_model(mspeed);
	speed_texture = load_3d_model_texture(speed_image_data, speed_widt, speed_heigh, "jpg");

	data_retrieved_flag = 1; //data is already retrieved, no need of retrieving it again
}

glm::mat4 rotation_matrix = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
glm::mat4 sphere_scale    = glm::scale(glm::vec3(0.05f, 0.0f, 0.02f));
											//y			   //x positive = increase width
glm::mat4 bar_scale       = glm::scale(glm::vec3(0.007f, 0.0f, 0.7f));
glm::mat4 brick_scale     = glm::scale(glm::vec3(0.03f, 0.07f, 0.07f));
glm::mat4 speed_scale     = glm::scale(glm::vec3(0.035f, 0.05f, 0.15f));//(0.014f, -0.26f, 0.07f));

glm::mat4 view_matrix	= glm::lookAt(glm::vec3(0, 1, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
   
void mrender()
{
	if (data_retrieved_flag == 0)
		retrieve_models_data();

	if (total_hit_bricks == max_hit_bricks)
	{
		reset_hit_list(hit_list, bricks_to_be_rendered, total_hit_bricks);	
		//LOG_INFO("total_hit_bricks: %d", total_hit_bricks);
	}
		
	
	{
		angle += 0.01f;
		if (angle > 1) 
				angle = 0;
		
		glClearColor(((float)x_down) / engine_width, angle,	((float)y_down) / engine_height, 1);
	}	
	
	//engine_height = 1014, engine_width = 1810;
	//default width/height ratio
	float default_ratio = 1810.0f/1080.0f;
	float target_ratio = (float)engine_width / (float)engine_height;
	float perspective_angle = (target_ratio / default_ratio) * 74.0f;
		
	glm::mat4 projection_matrix  = glm::perspective(glm::radians(perspective_angle), (float)(engine_height) / (float)(engine_width), 0.1f, 10.0f);

	
	glEnable(GL_DEPTH_TEST);																				
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);														
	
	//Shader program
	GLuint shader_program = load(mvshader, mfshader);
	glUseProgram(shader_program);																			

	int indices_size;
	
	glm::mat4 translation;
	glm::mat4 model_matrix;
	glm::mat4 model_view_projection;

	sphere_coordinates my_spcoords;	
		
	//bar
	{
		Setup_VertexAttribPointers((void*)&bar_vertices.positions[0], (void*)&bar_vertices.texture_coords[0]);
		Bind_Texture(GL_TEXTURE_2D, bar_texture, GL_TEXTURE0);
		
		 						//bar 1 width = |13| i.e the difference btwn sphere coordinates of both ends of the bar;
														                     //x negative = right  bar 1:min(-0.46f)  max(0.46f)
		translation = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 0.0f, bar_position));

		model_matrix = rotation_matrix * bar_scale * translation;

		model_view_projection = projection_matrix *  view_matrix * model_matrix;

		glUniformMatrix4fv(glGetUniformLocation(shader_program, "mvp"), 1, GL_FALSE, &model_view_projection[0][0]);

		indices_size = mbar.LoadedIndices.size();
		glDrawElements(GL_TRIANGLES, indices_size, GL_UNSIGNED_SHORT, &mbar.LoadedIndices[0]);																	 
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
		
	//sphere
	{
		Setup_VertexAttribPointers((void*)&sphere_vertices.positions[0], (void*)&sphere_vertices.texture_coords[0]);
		Bind_Texture(GL_TEXTURE_2D, sphere_texture, GL_TEXTURE0);
		
		my_spcoords = sphere_coords(bar_position);
														      //y positive = down     x negative = right
		translation = glm::translate(glm::mat4(1.0f), glm::vec3(my_spcoords.y, 0.0f, my_spcoords.x)); //my_spcoords.y, 0.0f, my_spcoords.x

		model_matrix = rotation_matrix * sphere_scale *  translation;
			
		model_view_projection = projection_matrix * view_matrix * model_matrix;

		glUniformMatrix4fv(glGetUniformLocation(shader_program, "mvp"), 1, GL_FALSE, &model_view_projection[0][0]);

		indices_size = msphere.LoadedIndices.size();
		glDrawElements(GL_TRIANGLES, indices_size, GL_UNSIGNED_SHORT, &msphere.LoadedIndices[0]);													

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
		

	//bricks
	{
		Setup_VertexAttribPointers((void*)&brick_vertices.positions[0], (void*)&brick_vertices.texture_coords[0]);
		Bind_Texture(GL_TEXTURE_2D, brick_texture, GL_TEXTURE0);
		int brickpositions_number = sizeof(brickpositions) / 12; //a glm::vec3 has 3 items each of which has 4 items thus the division by 3 * 4

		for (int brick_position = 0; brick_position < brickpositions_number; brick_position++)
		{
			if (hit_list[brick_position] == false)
			{		
				translation = glm::translate(glm::mat4(1.0f), brickpositions[brick_position]);

				model_matrix = rotation_matrix * brick_scale * translation;

				model_view_projection = projection_matrix * view_matrix * model_matrix;

				glUniformMatrix4fv(glGetUniformLocation(shader_program, "mvp"), 1, GL_FALSE, &model_view_projection[0][0]);

				indices_size = mbrick.LoadedIndices.size();
				glDrawElements(GL_TRIANGLES, indices_size, GL_UNSIGNED_SHORT, &mbrick.LoadedIndices[0]);																	 
			}
			
		}
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	

	//speed
	{
		Setup_VertexAttribPointers((void*)&speed_vertices.positions[0], (void*)&speed_vertices.texture_coords[0]);
		Bind_Texture(GL_TEXTURE_2D, speed_texture, GL_TEXTURE0);
															 //xpos		   //ypos
		translation = glm::translate(glm::mat4(1.0f), glm::vec3(12.0f, 0.0f, 3.2f));
		glm::mat4 srotation_matrix = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		model_matrix = srotation_matrix * speed_scale * translation;

		model_view_projection = projection_matrix * view_matrix * model_matrix;

		glUniformMatrix4fv(glGetUniformLocation(shader_program, "mvp"), 1, GL_FALSE, &model_view_projection[0][0]);

		indices_size = mspeed.LoadedIndices.size();
		glDrawElements(GL_TRIANGLES, indices_size, GL_UNSIGNED_SHORT, &mspeed.LoadedIndices[0]);																	 

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
		
}


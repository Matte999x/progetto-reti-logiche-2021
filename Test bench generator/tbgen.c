#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#define IMG_SIDE_MAX 128
#define IMG_DIM_MAX 128 * 128
#define MAX_PIXEL_VALUE 255

typedef struct {
	int dimX;
	int dimY;
	int imgIn[IMG_DIM_MAX];
	int imgOut[IMG_DIM_MAX];
}mem;

// tbgen.exe numTest [dimX dimY]
int main(int argc, char** argv){

    // generator parameters
	int numTest, dimX, dimY, maxDimX = IMG_SIDE_MAX, maxDimY = IMG_SIDE_MAX;
	bool randomDim = false;

    // user input
    printf("Number of images: ");
    scanf("%d", &numTest);

    if (numTest == 0){
        printf("0 Test benches generated\n");
        printf("Press ENTER to continue...\n");
        getchar();
        return 0;
    }

    printf("Image dimensions (e.g. dimX dimY) or 0 0 for random dimensions: ");
    scanf("%d %d", &dimX, &dimY);

    if (dimX == 0 && dimY == 0){
        randomDim = true;
        printf("Image dimension limits (e.g.  maxDimX maxDimY), 0 0 for unlimited dimensions: ");
        scanf("%d %d", &maxDimX, &maxDimY);
        if (maxDimX == 0)
            maxDimX = IMG_SIDE_MAX;
        if (maxDimY == 0)
            maxDimY = IMG_SIDE_MAX;
    }
    else if (dimX == 0 || dimY == 0){
        printf("One of the entered dimensions is 0\n");
        printf("Press ENTER to continue...\n");
        getchar();
        return 0;
    }

	if (randomDim)
		printf("Generating test bench with %d images with random dimensions (max %d x %d) to be equalized sequentially...\n", numTest, maxDimX, maxDimY);
	else
		printf("Generating test bench with %d images %d x %d to be equalized sequentially...\n", numTest, dimX, dimY);

    // --- MEMORY GENERATION ---

    srand(time(0));

    // RAM init
    mem *ram = (mem *)malloc(numTest * sizeof(mem));

    // dimensions generation
    if (randomDim)
        for (int i = 0; i < numTest; i++){
            ram[i].dimX = 1 + (rand() % maxDimX);
            ram[i].dimY = 1 + (rand() % maxDimY);
        }
    else
        for (int i = 0; i < numTest; i++){
            ram[i].dimX = dimX;
            ram[i].dimY = dimY;
        }

    // input image generation
    for (int i = 0; i < numTest; i++)
        for (int j = 0; j < ram[i].dimX * ram[i].dimY; j++)
            ram[i].imgIn[j] = rand() % (MAX_PIXEL_VALUE + 1);

    // expected output image computation
    for (int i = 0; i < numTest; i++){

        int minPixelValue = MAX_PIXEL_VALUE;
        int maxPixelValue = 0;

        for (int j = 0; j < ram[i].dimX * ram[i].dimY; j++){
            if (ram[i].imgIn[j] < minPixelValue)
                minPixelValue = ram[i].imgIn[j];
            if (ram[i].imgIn[j] > maxPixelValue)
                maxPixelValue = ram[i].imgIn[j];
        }

        int deltaValue = maxPixelValue - minPixelValue;
        int shiftLevel = 8 - (int)log2f((deltaValue + 1));

        for (int j = 0; j < ram[i].dimX * ram[i].dimY; j++){
            int tempPixel = ((ram[i].imgIn[j] - minPixelValue) << shiftLevel);
            if (tempPixel <= MAX_PIXEL_VALUE)
                ram[i].imgOut[j] = tempPixel;
            else
                ram[i].imgOut[j] = MAX_PIXEL_VALUE;
        }

    }


    // --- FILE WRITING ---

    // file creation
    FILE *outputFile;
    outputFile = fopen ("tb.vhd", "w");

    // header and generator constants
    fprintf (outputFile,
             "library ieee;\n"
             "use ieee.std_logic_1164.all;\n"
             "use ieee.numeric_std.all;\n"
             "use ieee.std_logic_unsigned.all;\n\n"
             "entity project_tb is\n"
             "end project_tb;\n\n"
             "architecture projecttb of project_tb is\n"
             "constant NUM_TEST : integer := %d;\n"
             "constant IMG_DIM : integer := %d;\n"
             "constant MEM_DIM : integer := 2 + IMG_DIM * 2;\n\n", numTest, maxDimX * maxDimY);

    // signals
    fprintf(outputFile,
            "constant c_CLOCK_PERIOD         : time := 15 ns;\n"
            "signal   tb_done                : std_logic;\n"
            "signal   mem_address            : std_logic_vector (15 downto 0) := (others => '0');\n"
            "signal   tb_rst                 : std_logic := '0';\n"
            "signal   tb_start               : std_logic := '0';\n"
            "signal   tb_clk                 : std_logic := '0';\n"
            "signal   mem_o_data,mem_i_data  : std_logic_vector (7 downto 0);\n"
            "signal   enable_wire            : std_logic;\n"
            "signal   mem_we                 : std_logic;\n\n"
            "signal	memoryNum : natural;\n\n"
            "type ram_type is array (NUM_TEST * MEM_DIM - 1 downto 0) of std_logic_vector(7 downto 0);\n\n");

    // RAM
    fprintf(outputFile,"signal RAM: ram_type := (\n");

    for (int i = 0; i < numTest; i++){
        fprintf(outputFile,"            %3d * MEM_DIM +     0 => std_logic_vector(to_unsigned(%4d, 8)),\n", i, ram[i].dimX);
        fprintf(outputFile,"            %3d * MEM_DIM +     1 => std_logic_vector(to_unsigned(%4d, 8)),\n", i, ram[i].dimY);
        for (int j = 0; j < ram[i].dimX * ram[i].dimY; j++)
            fprintf(outputFile,"            %3d * MEM_DIM + %5d => std_logic_vector(to_unsigned(%4d, 8)),\n", i, j + 2, ram[i].imgIn[j]);
    }
    fprintf(outputFile,"            others => (others =>'0'));\n\n");

    // mapping and auxiliary processes
    fprintf(outputFile,
        "component project_reti_logiche is\n"
        "port (\n"
        "      i_clk         : in  std_logic;\n"
        "      i_start       : in  std_logic;\n"
        "      i_rst         : in  std_logic;\n"
        "      i_data        : in  std_logic_vector(7 downto 0);\n"
        "      o_address     : out std_logic_vector(15 downto 0);\n"
        "      o_done        : out std_logic;\n"
        "      o_en          : out std_logic;\n"
        "      o_we          : out std_logic;\n"
        "      o_data        : out std_logic_vector (7 downto 0)\n"
        "      );\n"
        "end component project_reti_logiche;\n"
        "\n"
        "begin\n"
        "UUT: project_reti_logiche\n"
        "port map (\n"
        "          i_clk        => tb_clk,\n"
        "          i_start      => tb_start,\n"
        "          i_rst        => tb_rst,\n"
        "          i_data       => mem_o_data,\n"
        "          o_address    => mem_address,\n"
        "          o_done       => tb_done,\n"
        "          o_en         => enable_wire,\n"
        "          o_we         => mem_we,\n"
        "          o_data       => mem_i_data\n"
        ");\n"
        "\n"
        "p_CLK_GEN : process is\n"
        "begin\n"
        "   wait for c_CLOCK_PERIOD/2;\n"
        "   tb_clk <= not tb_clk;\n"
        "end process p_CLK_GEN;\n"
        "\n"
        "MEM : process(tb_clk)\n"
        "begin\n"
        "   if tb_clk'event and tb_clk = '1' then\n"
        "       if enable_wire = '1' then\n"
        "           if mem_we = '1' then\n"
        "               RAM(MEM_DIM * memoryNum + conv_integer(mem_address))   <= mem_i_data;\n"
        "               mem_o_data                                              <= mem_i_data after 1 ns;\n"
        "           else\n"
        "               mem_o_data <= RAM(MEM_DIM * memoryNum + conv_integer(mem_address)) after 1 ns;\n"
        "           end if;\n"
        "       end if;\n"
        "   end if;\n"
        "end process;\n"
        "\n");

    // main process
    fprintf(outputFile,
        "test : process is\n"
        "begin\n\n"
		"    wait for c_CLOCK_PERIOD;\n"
		"    tb_rst <= '1';\n"
		"    wait for c_CLOCK_PERIOD;\n"
		"    wait for 100 ns;\n"
		"    tb_rst <= '0';\n"
		"    wait for c_CLOCK_PERIOD;\n"
		"    wait for 100 ns;\n");

    for (int i = 0; i < numTest; i++){
        fprintf(outputFile,
			"    memoryNum <= %d;\n"
			"    wait for c_CLOCK_PERIOD;\n"
            "    tb_start <= '1';\n"
            "    wait for c_CLOCK_PERIOD;\n"
            "    wait until tb_done = '1';\n"
            "    wait for c_CLOCK_PERIOD;\n"
            "    tb_start <= '0';\n"
            "    wait until tb_done = '0';\n\n", i);
        for (int j = 0; j < ram[i].dimX * ram[i].dimY; j++)
            fprintf(outputFile,"    assert RAM(%5d + MEM_DIM * memoryNum) = std_logic_vector(to_unsigned(%4d, 8)) report \"TEST FAILED (WORKING ZONE). Expected %4d found \" & integer'image(to_integer(unsigned(RAM(%5d + MEM_DIM * memoryNum))))  severity failure;\n"
                    , 2 + (ram[i].dimX * ram[i].dimY) + j, ram[i].imgOut[j], ram[i].imgOut[j], 2 + (ram[i].dimX * ram[i].dimY) + j);
        fprintf(outputFile,"\n");
    }

    fprintf(outputFile,
        "   assert false report \"Simulation Ended! TEST PASSED\" severity failure;\n"
        "end process test;\n\n"
        "end projecttb;\n");


    free(ram);

    fclose(outputFile);

	printf("File generated\n");
	printf("Press ENTER to continue...\n");
	getchar();
	return 0;
}

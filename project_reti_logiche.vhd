library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;


entity project_reti_logiche is
    port (
        i_clk       : in std_logic;
        i_rst       : in std_logic;
        i_start     : in std_logic;
        i_data      : in std_logic_vector(7 downto 0);
        o_address   : out std_logic_vector(15 downto 0);
        o_done      : out std_logic;
        o_en        : out std_logic;
        o_we        : out std_logic;
        o_data      : out std_logic_vector (7 downto 0)
    );
end project_reti_logiche;


architecture project_reti_logiche_architecture of project_reti_logiche is

    type state_type is (READY,
                        READ_X_DIM,
                        READ_Y_DIM,
                        FIND_MIN_MAX,
                        EQUALIZE_AND_WRITE,
                        GET_NEXT_PIXEL_FOR_EQUALIZATION,
                        DONE);
                        
    signal current_state, next_state                                                    : state_type;
    signal pixel_count, next_pixel_count, counter, next_counter                         : unsigned(15 downto 0);
    signal min_pixel_value, max_pixel_value, next_min_pixel_value, next_max_pixel_value : unsigned(7 downto 0);
    
    
begin

    -- Registers process
    STATE_REG: process(i_clk, i_rst)
    begin
        if i_rst = '1' then
            current_state <= READY;
        elsif rising_edge(i_clk) then            
            pixel_count <= next_pixel_count;
            counter <= next_counter;
            min_pixel_value <= next_min_pixel_value;
            max_pixel_value <= next_max_pixel_value;
            current_state <= next_state;
        end if;    
    end process;    
    
    -- Combinational process
    COMB_PROC: process(current_state,
                        i_start,
                        i_data,                        
                        pixel_count,
                        counter,
                        min_pixel_value,
                        max_pixel_value)
                                                
    variable new_pixel_value            : unsigned(15 downto 0);
    variable current_pixel_value        : unsigned(7 downto 0);
    variable delta_value                : unsigned(7 downto 0);
    variable shift_level                : natural range 0 to 8;
    
    begin
    
        -- outputs initialization
        o_done <= '0';
        o_en <= '1'; -- RAM is always used, except in START and DONE states
        o_we <= '0';
        o_address <= std_logic_vector(to_unsigned(0, o_address'length));
        o_data <= std_logic_vector(to_unsigned(0, o_data'length));        
        -- internal signals initialization
        next_counter <= counter;
        next_pixel_count <= pixel_count;
        next_min_pixel_value <= min_pixel_value;
        next_max_pixel_value <= max_pixel_value;        
        
        case current_state is
        
            when READY =>            
                next_min_pixel_value <= to_unsigned(255, min_pixel_value'length);
                next_max_pixel_value <= to_unsigned(0, max_pixel_value'length);                                
                if i_start = '1' then
                    next_state <= READ_X_DIM;
                else
                    o_en <= '0';
                    next_state <= READY;
                end if;
                
            when READ_X_DIM =>
                -- total number of pixel count (reading first dimension)
                next_pixel_count <= resize(unsigned(i_data), pixel_count'length);
                -- set outputs for reading second dimension
                o_address <= std_logic_vector(to_unsigned(1, o_address'length));
                next_state <= READ_Y_DIM;
            
            when READ_Y_DIM =>
                -- total number of pixel count (reading second dimension)
                next_pixel_count <= resize(pixel_count * unsigned(i_data), pixel_count'length);
                -- set outputs for comparing loop (find min and max)
                next_counter <= to_unsigned(1, next_counter'length);
                o_address <= std_logic_vector(to_unsigned(2, o_address'length));
                next_state <= FIND_MIN_MAX;               

            when FIND_MIN_MAX =>
                if counter <= pixel_count then
                    -- more pixels to be compared
                    current_pixel_value := unsigned(i_data);
                    if current_pixel_value < min_pixel_value then
                        next_min_pixel_value <= current_pixel_value;
                    end if;
                    if current_pixel_value > max_pixel_value then
                        next_max_pixel_value <= current_pixel_value;
                    end if;                    
                    next_counter <= counter + 1;
                    o_address <= std_logic_vector(counter + 2);
                    next_state <= FIND_MIN_MAX;
                else
                    -- all pixels compared, set outputs for reading and equalizing first pixel
                    next_counter <= to_unsigned(1, next_counter'length);
                    o_address <= std_logic_vector(to_unsigned(2, o_address'length));                    
                    next_state <= EQUALIZE_AND_WRITE;
                end if;
                           
            when EQUALIZE_AND_WRITE =>
                if counter <= pixel_count then
                    -- reading pixel
                    current_pixel_value := unsigned(i_data);
                    -- computing equalized pixel value
                    delta_value := max_pixel_value - min_pixel_value;
                    if delta_value = 0 then
                        shift_level := 8;
                    elsif delta_value <= 2 then
                        shift_level := 7;
                    elsif delta_value <= 6 then
                        shift_level := 6;
                    elsif delta_value <= 14 then
                        shift_level := 5;
                    elsif delta_value <= 30 then
                        shift_level := 4;
                    elsif delta_value <= 62 then
                        shift_level := 3;
                    elsif delta_value <= 126 then
                        shift_level := 2;
                    elsif delta_value <= 254 then
                        shift_level := 1;
                    else
                        shift_level := 0;
                    end if;
                    new_pixel_value := shift_left(resize(current_pixel_value - min_pixel_value, new_pixel_value'length), shift_level);
                    -- overflow check
                    if new_pixel_value <= 255 then
                        o_data <= std_logic_vector(new_pixel_value(o_data'length - 1 downto 0));
                    else
                        o_data <= std_logic_vector(to_unsigned(255, o_data'length));
                    end if;
                    -- set outputs for writing equalized pixel 
                    o_address <= std_logic_vector(counter + 1 + pixel_count);
                    o_we <= '1';
                    next_state <= GET_NEXT_PIXEL_FOR_EQUALIZATION;
                else
                    next_state <= DONE;
                end if;
                
			when GET_NEXT_PIXEL_FOR_EQUALIZATION =>
			    -- set outputs for reading next pixel
                o_address <= std_logic_vector(counter + 2);
                next_counter <= counter + 1;
                next_state <= EQUALIZE_AND_WRITE;
                    
            when DONE =>
                o_en <= '0';
                o_done <= '1';
                if i_start = '0' then
                    next_state <= READY;
                else
                    next_state <= DONE;
                end if;
            
        end case;
        
    end process;
    
end project_reti_logiche_architecture;

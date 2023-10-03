//Terminal_Viewer.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <random>
#include <array>
#include <cstdio>
#include <thread>
#include <sstream>
#include <cstdint>

#include "../DCMA_Definitions.h"

#if defined(DCMA_HAS_UNISTD) && DCMA_HAS_UNISTD
    #include <unistd.h>
#endif
#if defined(DCMA_HAS_TERMIOS) && DCMA_HAS_TERMIOS
    #include <termios.h>
#endif
#if defined(DCMA_HAS_FCNTL) && DCMA_HAS_FCNTL
    #include <fcntl.h>
#endif

#include "../Colour_Maps.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"

#include "../Font_DCMA_Minimal.h"
#include "../DCMA_Version.h"
#include "../File_Loader.h"
#include "../Script_Loader.h"
#include "../Thread_Pool.h"

#include "YgorMath.h"
#include "YgorImages.h"


// Move terminal cursor.
//
// Note: accepts zero-based terminal 'pixel' coordinates, with (0,0) being top-left and (1,0) being below (0,0).
static
inline
void move_cursor_to(std::ostream &os, int row, int col){
    os << "\x1B[" << (row+1) << ";" << (col+1) << "H";
    return;
}

static
void terminal_enable_raw_mode(){
#if defined(DCMA_HAS_CSTDIO_FILENO) && DCMA_HAS_CSTDIO_FILENO \
 && defined(DCMA_HAS_TERMIOS) && DCMA_HAS_TERMIOS
    const auto fd = fileno(stdin);
    struct termios term_settings;
    if(tcgetattr(fd, &term_settings) < 0){
        throw std::runtime_error("Unable to get current terminal settings");
    }

    // Disable 'canonical' mode. Also disable terminal echo, since we usually don't want the keypress to be displayed.
    term_settings.c_lflag &= ~ICANON;
    term_settings.c_lflag &= ~ECHO;
    term_settings.c_cc[VMIN] = 1;
    term_settings.c_cc[VTIME] = 0;
    if(tcsetattr(fd, TCSANOW, &term_settings) < 0){
        throw std::runtime_error("Unable to disable terminal's canonical mode");
    }
#else
    // Currently no need for a workaround -- we accept if canonical mode is not possible and carry on.
#endif
    return;
}

static
void terminal_disable_raw_mode(){
#if defined(DCMA_HAS_CSTDIO_FILENO) && DCMA_HAS_CSTDIO_FILENO \
 && defined(DCMA_HAS_TERMIOS) && DCMA_HAS_TERMIOS
    const auto fd = fileno(stdin);
    struct termios term_settings;
    if(tcgetattr(fd, &term_settings) < 0){
        throw std::runtime_error("Unable to get current terminal settings");
    }

    // Re-enable 'canonical' mode and terminal echo.
    term_settings.c_lflag |= ICANON;
    term_settings.c_lflag |= ECHO;
    if(tcsetattr(fd, TCSADRAIN, &term_settings) < 0){
        throw std::runtime_error("Unable to enable terminal's canonical mode");
    }
#else
    // Currently no need for a workaround -- we accept if canonical mode is not possible and carry on.
#endif
    return;
}

static
void disable_blocking_stdin(){
#if defined(DCMA_HAS_CSTDIO_FILENO) && DCMA_HAS_CSTDIO_FILENO \
 && defined(DCMA_HAS_FCNTL) && DCMA_HAS_FCNTL
    const auto fd = fileno(stdin);
    auto flags = fcntl(fd, F_GETFL);
    if( flags < 0){
        throw std::runtime_error("Unable to get current file status for stdin");
    }
    flags |= O_NONBLOCK;
    if( fcntl(fd, F_SETFL, flags) < 0 ){
        throw std::runtime_error("Unable to set stdin to non-blocking mode");
    }
#else
    // Avoid deadlocks by throwing if this is not available.
    // (Is there a workaround?)
    throw std::runtime_error("No way to disable blocking stdin");
#endif
    return;
}

static
void enable_blocking_stdin(){
#if defined(DCMA_HAS_CSTDIO_FILENO) && DCMA_HAS_CSTDIO_FILENO \
 && defined(DCMA_HAS_FCNTL) && DCMA_HAS_FCNTL
    const auto fd = fileno(stdin);
    auto flags = fcntl(fd, F_GETFL);
    if( flags < 0){
        throw std::runtime_error("Unable to get current file status for stdin");
    }
    flags &= ~O_NONBLOCK;
    if( fcntl(fd, F_SETFL, flags) < 0 ){
        throw std::runtime_error("Unable to set stdin to blocking mode");
    }
#else
    throw std::runtime_error("No way to enable blocking stdin");
#endif
    return;
}

static
char read_unbuffered_raw_char(){
#if defined(DCMA_HAS_CSTDIO_FILENO) && DCMA_HAS_CSTDIO_FILENO \
 && defined(DCMA_HAS_UNISTD) && DCMA_HAS_UNISTD
    // First check if it is possible to type a character. For example, if output is redirected than the user probably
    // cannot access stdin.
    if(!::isatty(fileno(stdout))){
        throw std::runtime_error("Stdout is redirected, so assuming terminal cannot be access for input");
    }
#endif

    char c = '\0';
    terminal_enable_raw_mode();
    bool read_ok = !!std::cin.read(&c,1); // Note: this read is blocking!
    terminal_disable_raw_mode();

    // If a problem was encountered, attempt to reset the terminal state.
    if(!read_ok){
        throw std::runtime_error("Unable to read keypress from terminal");
    }
    return c;
}

static
bool terminal_supports_24bit_colour(){

    // In general, there is no way to completely ensure truecolour is possible.
    //
    // The most robust way is to use terminal capabilities databases, but these can become outdated and drag in a lot of
    // baggage.
    //
    // Another way is to query the terminal directly, to see if we can set a colour and then read the same colour using
    // DECRQSS queries. Unfortunatrly here are several 'gotchas' possible this way. First, not all terminals support the
    // query mechanism. Second, naively waiting for a response to the query will block waiting for input. Standard C++
    // provides no reasonable way to timeout a read, so we have to use OS-specific functionality to accommodate. Even
    // with reads that timeout, it's hard to synchronize the write and terminal response. Furthermore, it's also tricky
    // to ensure no other threads leak data to stdout that could confuse the terminal.
    //
    // Another way is to rely on environment variables. This technique mostly won't work due to evolving support in
    // terminals, and the complexity of nesting (e.g., GNU screen + xterm).
    //
    // The rationale for the approach taken here is:
    // 1. Reject including terminal capability databases, since they can actively impede better methods (e.g.,
    //    direct capability querying).
    // 2. Aim to use the same solution on as many platforms as possible (e.g., minimize use of OS-specific code).
    // 3. Truecolour (24-bit colour) was implemented by most (all?) major Linux terminals around 2017-2019, so assume
    //    that available terminals are recent enough to have support (e.g., if xterm is being used, assume it has 24-bit
    //    colour support rather than attempting to query).


    // First approach: check environment variables for major terminals with known support.
    if(const char *term = std::getenv("TERM"); (nullptr != term)){
        std::string term_str(term);

        // Note: consult https://github.com//termstandard/colors for list of terminals supporting 24-bit colour.
        if(term_str.find("256color")     != std::string::npos) return true;

        if(term_str.find("xterm")        != std::string::npos) return true;
        if(term_str.find("rxvt-unicode") != std::string::npos) return true;
        if(term_str.find("screen")       != std::string::npos) return true;
        // ... add more here as needed ...
    }
    if(const char *colorterm = std::getenv("COLORTERM"); (nullptr != colorterm)){
        std::string colorterm_str(colorterm);
        if(colorterm_str.find("truecolor")    != std::string::npos) return true;
        // ... add more here as needed ...
    }


    // Second approach: query the terminal directly.
    //
    // Note: I can't find a terminal to even test this on. Forgoing until needed.
/*

    // Exhaust std::cin buffer.
    // ... TODO ...

    terminal_enable_raw_mode();
    //disable_blocking_stdin();  // Note: might not be supported by terminal, so don't rely on it.

    const auto enquire = [](){
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        
        // Note: use a condition variable here to synchronize better. TODO.

        // Note: It would be best to attempt a variety of colours here to confirm they're all supported. TODO.
        const std::string set_colour_1("\x1B[48;2;123;213;147m");
        const std::string query_colour("\x1BP$qm\x1B\\");
        const std::string reset_colour("\x1B[0m");
        std::cout.write(set_colour_1.data(), set_colour_1.size());
        std::cout.write(query_colour.data(), query_colour.size());
        std::cout.write(reset_colour.data(), reset_colour.size());
        std::cout.flush();
        return;
    };


    const auto fd = fileno(stdin);
    std::string response_1;
    bool read_successful = false;
    while(true){
        // Check if data is available to be read.
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv = { 0 };
        tv.tv_sec = 1;
        tv.tv_usec = 50'000;

        const auto res = ::select(1, &rfds, nullptr, nullptr, &tv);
        //std::cerr << "Debug: loop resulted in select() = " << res << ". The response so far has " << response_1.size() << " bytes." << std::endl;

        // If an error occurred, treat the terminal as though it lacks support.
        if(res < 0){
            read_successful = false;
            break;

        // If there is data waiting, read it.
        }else if(0 < res){
            // Note that despite using select(), it's possible for this read to block.
            // This can happen due to spurious wake-ups. See `man select`.
            char c = '\0';
            if(!std::cin.read(&c, 1)){
            //const auto amount = ::read(fd, &c, 1);
            //if(amount != 1){
                read_successful = false;
                break;
            }
            response_1.push_back(c);
            if(25 < response_1.size()){
                read_successful = false;
                break;
            }

            // Check if a response terminator is detected. If so, stop reading.
            if(response_1.back() == '\\'){ // TODO! Is this the correct terminator???
                read_successful = true;
                break;
            }


        // If no prior data has been read and the select timed out, then treat the terminal as though it lacks
        // support.
        }else if((res == 0) && response_1.empty()){
            read_successful = false;
            break;

        // If we previously read data, but have run out, then we have likely read the entire response.
        }else if(!response_1.empty()){
            read_successful = true;
            break;

        }else{
            read_successful = false;
            break;
        }
    }
    //enable_blocking_stdin();
    terminal_disable_raw_mode();
    if(!read_successful) return false;


    std::cerr << std::endl;
    std::cerr << std::endl;
    std::cerr << std::endl;
    for(const auto &c : response_1){
        std::cerr << static_cast<int>(c) << " ";
        if(std::isprint(static_cast<int>(c))) std::cerr << "(" << c << ") ";
        std::cerr << std::endl;
    }
    // Sample output:
    //    27 
    //    80 (P) 
    //    49 (1) 
    //    36 ($) 
    //    114 (r) 
    //    48 (0) 
    //    59 (;) 
    //    52 (4) 
    //    56 (8) 
    //    58 (:) 
    //    50 (2) 
    //    58 (:) 
    //    58 (:) 
    //    49 (1) 
    //    58 (:) 
    //    50 (2) 
    //    58 (:) 
    //    51 (3) 
    //    109 (m) 
    //    27 
    //    92 (\) 
    //    108 (l) 


    // Try parse the response. Note that colons or semicolons might be present. Also, additional information could, in
    // principle, be present.
    if( (response_1 == "\x1BP1$r48;2;1;2;3m\x1B[")
    ||  (response_1 == "\x1BP1$r48:2:1:2:3m\x1B[") ){
        //std::cerr << "Support detected" << std::endl;
        return true;
    }
    //std::cerr << "Support NOT detected" << std::endl;
*/
    return false;
}

enum class terminal_colour_mode_t {
    bit24,
    bit6,
    step24,

    step5,
    numbers,
    punctuation,
};

static bool terminal_supports_ansi(terminal_colour_mode_t colour_mode){
    return   (colour_mode == terminal_colour_mode_t::bit24)
          || (colour_mode == terminal_colour_mode_t::bit6)
          || (colour_mode == terminal_colour_mode_t::step24);
}


void draw_image( std::ostream &os,
                 const planar_image<float, double> &img, 
                 const int64_t term_draw_pow_row,
                 const int64_t term_draw_pos_col,
                 const int64_t max_square_size,
                 const std::function<ClampedColourRGB(double)> &colour_map,
                 terminal_colour_mode_t colour_mode ){

    const bool supports_ansi = terminal_supports_ansi(colour_mode);

    // Rescale to help mitigate edge-cases and account (partially) for aspect ratio correction.
    const auto nearest_even_number = [](int64_t i){
        return  static_cast<int64_t>( 2.0 * std::round(0.5 * static_cast<double>(i)) );
    };
    const auto [min, max] = img.minmax();
    const int64_t channel = 0;

    planar_image<float, double> scaled_img;
    const auto aspect = (img.pxl_dy * img.rows) / (img.pxl_dx * img.columns);
    auto new_cols = nearest_even_number(max_square_size);
    auto new_rows = static_cast<int64_t>( std::clamp<double>( 2.0 * std::floor( aspect * new_cols * 0.5), 1.0, 5.0 * new_cols) );
    if(new_cols < new_rows){
        new_rows = max_square_size;
        new_cols = static_cast<int64_t>( std::clamp<double>( 2.0 * std::floor( (1.0 / aspect) * new_rows * 0.5), 1.0, 5.0 * new_rows) );
    }
    if( (max_square_size < new_rows)
    ||  (max_square_size < new_cols) ){
        throw std::runtime_error("Unable to fit image within designated area");
    }

    scaled_img.init_buffer(new_rows, new_cols, 1);
    scaled_img.init_spatial(1.0, 1.0, 0.0, vec3<double>(0.0, 0.0, 0.0), vec3<double>(0.0, 0.0, 0.0));
    scaled_img.init_orientation(vec3<double>(1.0, 0.0, 0.0), vec3<double>(0.0, 1.0, 0.0));

    scaled_img.apply_to_pixels([&](int64_t row, int64_t col, int64_t chnl, float &val){
        const auto r_f = (static_cast<double>(row) / static_cast<double>(new_rows)) * img.rows;
        const auto c_f = (static_cast<double>(col) / static_cast<double>(new_cols)) * img.columns;
        val = img.bilinearly_interpolate_in_pixel_number_space(
                  std::clamp<double>(r_f, 0.0, static_cast<double>(img.rows)-1.0),
                  std::clamp<double>(c_f, 0.0, static_cast<double>(img.columns)-1.0),
                  chnl );
    });

    const auto map_to_24bit_colour = [](const ClampedColourRGB &rgb){
        const auto r = std::clamp<int>(static_cast<int>( std::round(0.0 + (255.0 - 0.0) * rgb.R) ), 0, 255);
        const auto g = std::clamp<int>(static_cast<int>( std::round(0.0 + (255.0 - 0.0) * rgb.G) ), 0, 255);
        const auto b = std::clamp<int>(static_cast<int>( std::round(0.0 + (255.0 - 0.0) * rgb.B) ), 0, 255);
        return std::make_tuple(r, g, b);
    };
    const auto map_to_6bit_colour_code = [](const ClampedColourRGB &rgb){
        // 6-bit colour embedded within 8-bit ANSI codes. Note that this colour code is specific to ANSI terminals.
        const auto r = std::clamp<int>(static_cast<int>( std::round(0.0 + (5.0 - 0.0) * rgb.R) ), 0, 5);
        const auto g = std::clamp<int>(static_cast<int>( std::round(0.0 + (5.0 - 0.0) * rgb.G) ), 0, 5);
        const auto b = std::clamp<int>(static_cast<int>( std::round(0.0 + (5.0 - 0.0) * rgb.B) ), 0, 5);
        return (6 * 6 * r + 6 * g + b + 16);
    };
    const auto map_to_24step_colour_code = [](float intensity){
        // 24-step grayscale embedded within 8-bit ANSI codes. Note that this colour code is specific to ANSI terminals.
        const auto g = std::clamp<int>(static_cast<int>( std::round(232.0 + (255.0 - 232.0) * intensity) ), 232, 255);
        return g;
    };
    const auto linear_glpyh_map = [](float intensity, const std::vector<std::string> &glyphs){
        // Given an ordered list of glpyhs and a number [0:1], figure out which glyph the number maps to.
        // This is essentially a histogram binning routine.
        const auto width = 1.0 / static_cast<double>(glyphs.size());
        for(size_t i = 0; i < glyphs.size(); ++i){
            const auto upper_threshold = static_cast<double>(i+1) * width;
            if(intensity < upper_threshold) return glyphs[i];
        }
        return glyphs.back();
    };
    const auto map_to_shade_glyph = [&linear_glpyh_map](float intensity){
        // Assume the terminal does not support colour, but is able to display unicode correctly.
        return linear_glpyh_map(intensity, { " ",    // Empty space.
                                             "░",    // Unicode U+2591 Light Shade.
                                             "▒",    // Unicode U+2592 Medium Shade.
                                             "▓",    // Unicode U+2593 Dark Shade.
                                             "█" }); // Unicode U+2588 Full Block.
    };
    const auto map_to_ascii_number_glyph = [&linear_glpyh_map](float intensity){
        // Assume the terminal does not support colour or unicode, or the font only supports basic ASCII characters.
        return linear_glpyh_map(intensity, { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" });
    };
    const auto map_to_ascii_punctuation_glyph = [&linear_glpyh_map](float intensity){
        // Assume the terminal does not support colour or unicode, or the font only supports basic ASCII characters.
        return linear_glpyh_map(intensity,
                // Subjectively sorted into order of apparent brightness on machine at time of writing.
                { " ",".","-","~","+","c","o","x","=","/","?","$","%","&","#","@","A","X","M"});
    };

    // Note: we split a terminal character into an upper and lower rectangular block. This is done because common
    // terminal fonts proportions are (roughly) twice as tall as they are wide. Splitting this way helps normalize the
    // aspect ratio presented to the user.
    const auto emit_vert_split_colours = [&](std::ostream &os, double upper_intensity, double lower_intensity){
        //const auto cm = ColourMap_Linear(intensity);
        //const auto cm = ColourMap_Magma(intensity);
        const auto upper_cm = ColourMap_Viridis(upper_intensity);
        const auto lower_cm = ColourMap_Viridis(lower_intensity);

        // 24-bit colour.
        if(colour_mode == terminal_colour_mode_t::bit24){
            const auto [upper_r, upper_g, upper_b] = map_to_24bit_colour(upper_cm);
            const auto [lower_r, lower_g, lower_b] = map_to_24bit_colour(lower_cm);
            os << "\x1B[38;2;" << upper_r << ";" << upper_g << ";" << upper_b << "m"; // Foreground colour.
            os << "\x1B[48;2;" << lower_r << ";" << lower_g << ";" << lower_b << "m"; // Background colour.
            os << R"***(▀)***";
            os << "\x1B[0m"; // Reset terminal colours at current position.

        // 6-bit colour embedded within 8-bit ANSI codes (i.e., should be supported by 8-bit terminals).
        }else if(colour_mode == terminal_colour_mode_t::bit6){
            os << "\x1B[38;5;" << map_to_6bit_colour_code(upper_cm) << "m"; // Foreground colour.
            os << "\x1B[48;5;" << map_to_6bit_colour_code(lower_cm) << "m"; // Background colour.
            os << R"***(▀)***";
            os << "\x1B[0m"; // Reset terminal colours at current position.

        // 24-step grayscale embedded within 8-bit ANSI codes (i.e., should be supported by 8-bit terminals).
        }else if(colour_mode == terminal_colour_mode_t::step24){
            os << "\x1B[38;5;" << map_to_24step_colour_code(upper_intensity) << "m"; // Foreground colour.
            os << "\x1B[48;5;" << map_to_24step_colour_code(lower_intensity) << "m"; // Background colour.
            os << R"***(▀)***";
            os << "\x1B[0m"; // Reset terminal colours at current position.

        // No colour or terminal support, but able to display unicode 'shade' glyphs.
        }else if(colour_mode == terminal_colour_mode_t::step5){
            const auto avg_intensity = (lower_intensity + upper_intensity) * 0.5;
            os << map_to_shade_glyph(avg_intensity);

        // No colour or terminal support and not able to display unicode glyphs. Truly a basic, minimal experience.
        }else if(colour_mode == terminal_colour_mode_t::numbers){
            const auto avg_intensity = (lower_intensity + upper_intensity) * 0.5;
            os << map_to_ascii_number_glyph(avg_intensity);

        }else if(colour_mode == terminal_colour_mode_t::punctuation){
            const auto avg_intensity = (lower_intensity + upper_intensity) * 0.5;
            os << map_to_ascii_punctuation_glyph(avg_intensity);

        }else{
            throw std::logic_error("Unrecognized colour mode");
        }
        return;
    };
    
    // Clear the screen.
    if(supports_ansi){
        os << "\x1B[2J";
    }else{
        os << std::string(5, '\n');
    }

    // Display the image.
    //
    // Note: consult https://en.wikipedia.org/wiki/ANSI_escape_code for more info.

    if(supports_ansi){
        move_cursor_to(os, term_draw_pow_row, term_draw_pos_col); // Move to reference position.
    }

    for(int64_t r = 0; r < scaled_img.rows; r += 2){
        if(supports_ansi){
            move_cursor_to(os, term_draw_pow_row + r/2, term_draw_pos_col);
        }else{
            os << std::string(term_draw_pos_col, ' ');
        }
        for(int64_t c = 0; c < scaled_img.columns; c++){
            const auto upper_val = scaled_img.value(r, c, channel);
            const auto upper_intensity = (min < max) ? (upper_val - min) / (max - min) : 1.0;

            const auto lower_val = scaled_img.value(r, c, channel);
            const auto lower_intensity = (min < max) ? (lower_val - min) / (max - min) : 1.0;

            emit_vert_split_colours(os, upper_intensity, lower_intensity);
        }

        // Also print a colour bar, since the colour ramp might not be smooth (depending on colours used).
        {
            if(supports_ansi){
                move_cursor_to(os, term_draw_pow_row + r/2, term_draw_pos_col + scaled_img.columns + 1);
            }else{
                os << " ";
            }
            const auto upper_intensity = 1.0 - static_cast<double>(r  )/static_cast<double>(scaled_img.rows-1);
            const auto lower_intensity = 1.0 - static_cast<double>(r+1)/static_cast<double>(scaled_img.rows-1);
            emit_vert_split_colours(os, upper_intensity, lower_intensity);
        }

        if(supports_ansi){
            os << "\x1B[0m"; // Reset terminal colours at current position.
            os.flush();
            move_cursor_to(os, term_draw_pow_row + scaled_img.rows/2, 0); // Move cursor to bottom.
        }else{
            os << std::endl;
        }
    }
    os.flush();
    return;
}


OperationDoc OpArgDocTerminal_Viewer(){
    OperationDoc out;
    out.name = "Terminal_Viewer";
    out.desc = 
        "Launch an interactive viewer inside a terminal/console.";

    out.args.emplace_back();
    out.args.back().name = "MaxImageLength";
    out.args.back().desc = "The maximum size images will be rendered."
                           " Note that aspect ratio scaling (which is approximate at best) may result in images"
                           " being displayed with smaller vertical and horizontal lengths."
                           " The optimal value depends on your screen resolution, font size, required visual"
                           " resolution, and, potentially, bandwidth.";
    out.args.back().default_val = "120";
    out.args.back().expected = true;
    out.args.back().examples = { "50", "78", "80", "120", "200" };

    out.args.emplace_back();
    out.args.back().name = "ColourMethod";
    out.args.back().desc = "Controls how images are displayed. The default, 'auto', will provide the highest"
                           " number of colour depth possible. However, automatic detection is hard so overrides"
                           " may be needed."
                           "\n\n'24-bit' provides the greatest colour depth, but is not supported by all terminals."
                           "\n\n'6-bit' provides a reasonable amount of colour depth, and is more widely supported."
                           "\n\n'24-steps' provides low-quality colour depth, but is almost universally available."
                           "\n\n'5-steps' displays intensity using unicode 'shade' blocks."
                           "\n\n'numbers' uses ASCII monochrome numbers (0-9) to display intensity."
                           "\n\n'punctuation' uses ASCII monochrome punctuation marks to display intensity.";
    out.args.back().default_val = "auto";
    out.args.back().expected = true;
    out.args.back().examples = { "auto", "24-bit", "6-bit", "24-steps", "5-steps", "numbers", "punctuation" };

    //out.args.emplace_back();
    //out.args.back() = IAWhitelistOpArgDoc();
    //out.args.back().name = "ImageSelection";
    //out.args.back().default_val = "last";

    return out;
}

bool Terminal_Viewer(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    //const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto MaxImageLength = std::stol(OptArgs.getValueStr("MaxImageLength").value());
    const auto ColourMethodStr = OptArgs.getValueStr("ColourMethod").value();
    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_auto    = Compile_Regex("^a?u?t?o?m?a?t?i?c?$");
    const auto regex_24bit   = Compile_Regex("^24[-_]?bi?t?$");
    const auto regex_6bit    = Compile_Regex("^6[-_]?bi?t?$");
    const auto regex_24step  = Compile_Regex("^24[-_]?st?e?p?s?$");
    const auto regex_5step   = Compile_Regex("^5[-_]?st?e?p?s?$");
    const auto regex_numbers = Compile_Regex("^nu?m?b?e?r?s?$");
    const auto regex_punct   = Compile_Regex("^pu?n?c?t?u?a?t?i?o?n?$");

    const auto term_draw_pow_row = 5UL;
    const auto term_draw_pos_col = 5UL;
    const auto max_square_size = MaxImageLength;

    terminal_colour_mode_t terminal_colour_mode = terminal_colour_mode_t::step24;
    if( std::regex_match(ColourMethodStr, regex_auto) ){
        if(terminal_supports_24bit_colour()){
            // Opt for 24-bit colour if available.
            terminal_colour_mode = terminal_colour_mode_t::bit24;
        }else{
            // Assume 6-bit colour is always available.
            terminal_colour_mode = terminal_colour_mode_t::bit6;
        }

    }else if( std::regex_match(ColourMethodStr, regex_24bit) ){
        terminal_colour_mode = terminal_colour_mode_t::bit24;
    }else if( std::regex_match(ColourMethodStr, regex_6bit) ){
        terminal_colour_mode = terminal_colour_mode_t::bit6;
    }else if( std::regex_match(ColourMethodStr, regex_24step) ){
        terminal_colour_mode = terminal_colour_mode_t::step24;
    }else if( std::regex_match(ColourMethodStr, regex_5step) ){
        terminal_colour_mode = terminal_colour_mode_t::step5;
    }else if( std::regex_match(ColourMethodStr, regex_numbers) ){
        terminal_colour_mode = terminal_colour_mode_t::numbers;
    }else if( std::regex_match(ColourMethodStr, regex_punct) ){
        terminal_colour_mode = terminal_colour_mode_t::punctuation;
    }else{
        throw std::invalid_argument("Colour method argument '"_s + ColourMethodStr + "' is not valid");
    }
    const bool supports_ansi = terminal_supports_ansi(terminal_colour_mode);

/*
    // Make a test image.
    planar_image<float, double> img;
    img.init_buffer(512, 512, 1);
    img.init_spatial(0.675, 0.675, 0.5, vec3<double>(0.0, 0.0, 0.0), vec3<double>(0.0, 0.0, 0.0));
    img.init_orientation(vec3<double>(0.0, 1.0, 0.0), vec3<double>(1.0, 0.0, 0.0));

    img.fill_pixels(0.0);
    {
        float curr = 0.0;
        for(int64_t r = 0; r < img.rows; ++r){
            for(int64_t c = 0; c < img.columns; ++c){
                for(int64_t h = 0; h < img.channels; ++h){
                    img.reference(r, c, h) = curr;
                    curr += 1.0;
                }
            }
        }
    }

    int64_t random_seed = 123456;
    std::mt19937 re( random_seed );
    std::uniform_real_distribution<> rd(0.0, 1.0); //Random distribution.
    {
        for(int64_t r = 0; r < img.rows; ++r){
            for(int64_t c = 0; c < img.columns; ++c){
                for(int64_t h = 0; h < img.channels; ++h){
                    img.reference(r, c, h) = rd(re);
                }
            }
        }
    }
*/

    // Image viewer state.
    int64_t img_array_num = -1; // The image array currently displayed.
    int64_t img_num = -1; // The image currently displayed.
    int64_t img_channel = -1; // Which channel to display.
    using img_array_ptr_it_t = decltype(DICOM_data.image_data.begin());
    using disp_img_it_t = decltype(DICOM_data.image_data.front()->imagecoll.images.begin());

    // Recompute image array and image iterators for the current image.
    const auto recompute_image_iters = [ &DICOM_data,
                                         &img_array_num,
                                         &img_num ](){
        std::tuple<bool, img_array_ptr_it_t, disp_img_it_t > out;
        std::get<bool>( out ) = false;

        // Set the current image array and image iters and load the texture.
        do{ 
            const auto has_images = DICOM_data.Has_Image_Data();
            if( !has_images ) break;
            if( !isininc(1, img_array_num+1, DICOM_data.image_data.size()) ) break;
            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
            if( img_array_ptr_it == std::end(DICOM_data.image_data) ) break;

            if( !isininc(1, img_num+1, (*img_array_ptr_it)->imagecoll.images.size()) ) break;
            auto disp_img_it = std::next((*img_array_ptr_it)->imagecoll.images.begin(), img_num);
            if( disp_img_it == std::end((*img_array_ptr_it)->imagecoll.images) ) break;

            if( (disp_img_it->channels <= 0)
            ||  (disp_img_it->rows <= 0)
            ||  (disp_img_it->columns <= 0) ) break;

            std::get<bool>( out ) = true;
            std::get<img_array_ptr_it_t>( out ) = img_array_ptr_it;
            std::get<disp_img_it_t>( out ) = disp_img_it;
        }while(false);
        return out;
    };

    // Advance to the specified Image_Array. Also resets necessary display image iterators.
    const auto advance_to_image_array = [ &DICOM_data,
                                          &img_array_num,
                                          &img_num ](const int64_t n){
            const int64_t N_arrays = DICOM_data.image_data.size();
            if((n < 0) || (N_arrays <= n)){
                throw std::invalid_argument("Unwilling to move to specified Image_Array. It does not exist.");
            }
            if(n == img_array_num){
                return; // Already at desired position, so no-op.
            }
            img_array_num = n;

            // Attempt to move to the Nth image, like in the previous array.
            //
            // TODO: It's debatable whether this is even useful. Better to move to same DICOM position, I think.
            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
            const int64_t N_images = (*img_array_ptr_it)->imagecoll.images.size();
            if(N_images == 0){
                throw std::invalid_argument("Image_Array contains no images. Refusing to continue");
            }
            img_num = (img_num < 0) ? 0 : img_num; // Clamp below.
            img_num = (N_images <= img_num) ? N_images - 1 : img_num; // Clamp above.
            return;
    };

    // Advance to the specified image in the current Image_Array.
    const auto advance_to_image = [ &DICOM_data,
                                    &img_array_num,
                                    &img_num ](const int64_t n){
            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
            const int64_t N_images = (*img_array_ptr_it)->imagecoll.images.size();

            if((n < 0) || (N_images <= n)){
                throw std::invalid_argument("Unwilling to move to specified image. It does not exist.");
            }
            if(n == img_num){
                return; // Already at desired position, so no-op.
            }
            img_num = n;
            return;
    };

    img_array_num = 0;
    img_num = 0;
    img_channel = 0;

    while(true){ 
        std::stringstream ss;

        auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
        const int N_arrays = (img_valid) ? DICOM_data.image_data.size() : 0;
        const int N_images = (img_valid) ? (*img_array_ptr_it)->imagecoll.images.size() : 0;
        if(img_valid){
            draw_image( ss,
                        (*disp_img_it),
                        term_draw_pow_row,
                        term_draw_pos_col,
                        max_square_size,
                        &ColourMap_Viridis,
                        terminal_colour_mode );
            ss << "Displaying image array " << img_array_num << "/" << N_arrays << ", image " << img_num << "/" << N_images << std::endl;
        }else{
            ss << "No image data to display." << std::endl;
        }
        ss.flush();

        char k = '\0';
        {
            std::lock_guard<std::mutex> lock(ygor::g_term_sync);
            std::cout << ss.rdbuf();
            std::cout.flush();

            std::cout << "Action: ";
            std::cout.flush();
            if(supports_ansi){
                k = read_unbuffered_raw_char();
            }else{
                if(!std::cin.get(k)){
                    k = '\x1B';
                }
            }
            std::cout << std::endl;
        }
        if( (k == 'Q') || (k == 'q') || (k == '\x1B') ){ // Exit.
            break;
        }else if( img_valid && (k == 'N') ){
            img_array_num = (img_array_num + 1) % N_arrays;
            img_num = 0;
            img_channel = 0;
        }else if( img_valid && (k == 'P') ){
            img_array_num = (N_arrays + img_array_num - 1) % N_arrays;
            img_num = 0;
            img_channel = 0;
        }else if( img_valid && (k == 'n') ){
            img_num = (img_num + 1) % N_images;
            img_channel = 0;
        }else if( img_valid && (k == 'p') ){
            img_num = (N_images + img_num - 1) % N_images;
            img_channel = 0;
        }
    }

    return true;
}

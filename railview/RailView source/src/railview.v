
module frame_buffer (input clk,
                    input [12:0] address,
                    input [12:0] addr_internal,
                    input [1:0] colour,
                    input IE,
                    output reg [1:0] dataOut);
    /*
    reg [1:0] buffer [1 << 13:0];

    always @(posedge clk) begin
        if (!IE) begin
            buffer[address] <= colour;
        end

        dataOut <= buffer[addr_internal];
    end
    */
    assign dataOut = 2'b01;
    
endmodule

module palatte  (input clk,
                input switch,
                input OE,
                input [1:0] colour_low,
                output reg [23:0] colour_high);

    reg [23:0] red;
    reg [23:0] green;
    reg [23:0] blue;

    reg [1:0] active;

    always @(negedge switch) begin
        if (active == 2'b00) begin
            active <= 2'b01;
            red <= 24'hFFFFFF;
            green <= 24'hFFFFFF;
            blue <= 24'hFFFFFF;
        end else if (active == 2'b01) begin
            active <= 2'b10;
            red <= 24'hFFB3BA;
            green <= 24'hBAFFC9;
            blue <= 24'hBAE1FF;
        end else if (active == 2'b10) begin
            active <= 2'b11;
            red <= 24'hFFFFFF;
            green <= 24'hFFFFFF;
            blue <= 24'hFFFFFF;
        end else if (active == 2'b11) begin
            active <= 2'b00;
            red <= 24'hFF0000;
            green <= 24'h00FF00;
            blue <= 24'h0000FF;
        end
    end

    always @(posedge OE) begin
        if (colour_low == 2'b00) begin
            colour_high <= 24'h000000;
        end else if (colour_low == 2'b01) begin
            colour_high <= red;
        end else if (colour_low == 2'b10) begin
            colour_high <= green;
        end else if (colour_low == 2'b11) begin
            colour_high <= blue;
        end
    end

endmodule

module top (input clk,
            input PALATTE_SWITCH,
            input [12:0] ADDRESS,
            input [1:0] COLOUR,
            input IE);

    palatte palatte_1 (
        .clk(clk),
        .switch(PALATTE_SWITCH)
    );

    frame_buffer buffer (
        .clk(clk),
        .address(ADDRESS),
        .colour(COLOUR),
        .IE(IE)
    );

endmodule
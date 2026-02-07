module frame_buffer (input clk,
                    input [12:0] address,
                    input [12:0] addr_internal,
                    input [1:0] colour,
                    input IE,
                    input vblank_clear,
                    output reg [1:0] dataOut,
                    output reg [23:0] bgcolour,
                    output reg interrupt);
    reg [1:0] buffer [0:8191];
    reg writing;
    reg [2:0] delayClocks;
    reg IEp0;
    reg IEp1;
    reg [12:0] addressLatch;
    reg [1:0] datap0;
    reg [1:0] datap1;

    reg [1:0] dataw;
    reg [12:0] addrw;
    reg wf;

    reg clearing;
    reg [13:0] clearAddr;
    reg [5:0] interrupt_time;

    wire ieFall = IEp1 & ~IEp0;
    wire ieRise = ~IEp1 & IEp0;
    
    reg VBp0;
    reg VBp1;
    wire VBRise;

    always @(posedge clk) begin
        VBp0 <= vblank_clear;
        VBp1 <= VBp0;
    end

    assign VBRise = ~VBp1 & VBp0;
    

    always @(posedge clk) begin
        IEp0 <= IE;
        IEp1 <= IEp0;
    
        wf <= 1'b0;

        if (delayClocks > 3'h0) begin
            delayClocks <= delayClocks - 3'h1;
        end

        if (ieFall) begin
            if (delayClocks == 3'h0 && !writing) begin
                writing <= 1'b1;
                addressLatch <= address;
            end
        end
        if (ieRise && !clearing) begin
            delayClocks <= 3'h6;
            writing <= 1'b0;
            dataw <= datap1;
            addrw <= addressLatch;
            wf <= 1'b1;
        end

        if (clearing) begin
            addrw <= clearAddr;
            dataw <= 2'd0;
            wf <= 1'b1;
            clearAddr <= clearAddr + 14'd1;
            if (clearAddr >= 14'd8191) begin
                clearing <= 1'b0;
                interrupt_time <= 6'd54;
            end
        end

        if (wf) begin
            buffer[addrw] <= dataw;
        end
        
        if (writing) begin
            datap0 <= colour;
            datap1 <= datap0;
        end

        if (VBRise) begin
            clearing <= 1'b1;
            clearAddr <= 13'd0;
        end

        if (interrupt_time != 6'd0) begin
            interrupt <= 1'b0;
            interrupt_time <= interrupt_time - 6'd1;
        end else 
            interrupt <= 1'b1;

        bgcolour[7:0] <= {buffer[13'h1FFF], buffer[13'h1FFE], buffer[13'h1FFD], buffer[13'h1FFC]};
        bgcolour[15:8] <= {buffer[13'h1FFB], buffer[13'h1FFA], buffer[13'h1FF9], buffer[13'h1FF8]};
        bgcolour[23:16] <= {buffer[13'h1FF7], buffer[13'h1FF6], buffer[13'h1FF5], buffer[13'h1FF4]};

        dataOut <= buffer[addr_internal];
    end
endmodule

module palette_sw (output reg [1:0] palette,
                    input clk,
                    input sw); 

    reg [19:0] debounce;

    always @(posedge clk) begin
        if (debounce > 20'd0 & sw) 
            debounce <= debounce -1'b1;
        else if (!sw) 
            debounce <= 20'd1_000_000; 
    end
    
    always @(negedge sw) begin
        if (debounce == 20'd0) begin
            case (palette)
                2'b00 : palette <= 2'b01;
                2'b01 : palette <= 2'b10;
                2'b10 : palette <= 2'b11;
                2'b11 : palette <= 2'b00;
                default : palette <= 2'b00;
            endcase 
        end
    end

endmodule
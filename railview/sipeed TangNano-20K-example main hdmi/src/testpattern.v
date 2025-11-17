module testpattern
	(
		input              I_pxl_clk   ,//pixel clock
		input              I_rst_n     ,//low active 
		input      [2:0]   I_mode      ,//data select
		input      [7:0]   I_single_r  ,
		input      [7:0]   I_single_g  ,
		input      [7:0]   I_single_b  ,
		input      [11:0]  I_h_total   ,//hor total time 
		input      [11:0]  I_h_sync    ,//hor sync time
		input      [11:0]  I_h_bporch  ,//hor back porch
		input      [11:0]  I_h_res     ,//hor resolution
		input      [11:0]  I_v_total   ,//ver total time 
		input      [11:0]  I_v_sync    ,//ver sync time  
		input      [11:0]  I_v_bporch  ,//ver back porch  
		input      [11:0]  I_v_res     ,//ver resolution 
		input              I_hs_pol    ,//HS polarity 
		input              I_vs_pol    ,//VS polarity 
		output             O_de        ,   
		output reg         O_hs        ,
		output reg         O_vs        ,
		output     [7:0]   O_data_r    ,    
		output     [7:0]   O_data_g    ,
		output     [7:0]   O_data_b    ,

		input [1:0] pixel_colour,
		output reg [12:0] pixel_address,
		input [23:0] bgcolour,
		input [1:0] palette
	); 

	//====================================================
	localparam N = 5; //delay N clocks

	localparam	WHITE	= {8'd255 , 8'd255 , 8'd255 };//{B,G,R}
	localparam	YELLOW	= {8'd0   , 8'd255 , 8'd255 };
	localparam	CYAN	= {8'd255 , 8'd255 , 8'd0   };
	localparam	GREEN	= {8'd0   , 8'd255 , 8'd0   };
	localparam	MAGENTA	= {8'd255 , 8'd0   , 8'd255 };
	localparam	RED		= {8'd0   , 8'd0   , 8'd255 };
	localparam	BLUE	= {8'd255 , 8'd0   , 8'd0   };
	localparam	BLACK	= {8'd0   , 8'd0   , 8'd0   };
	
	//====================================================
	reg  [11:0]   V_cnt     ;
	reg  [11:0]   H_cnt     ;

	reg [6:0] V_px_cnt;
	reg [6:0] H_px_cnt;

	reg [2:0] V_line_step;
				
	wire          Pout_de_w    ;                          
	wire          Pout_hs_w    ;
	wire          Pout_vs_w    ;

	reg  [N-1:0]  Pout_de_dn   ;                          
	reg  [N-1:0]  Pout_hs_dn   ;
	reg  [N-1:0]  Pout_vs_dn   ;

	//----------------------------
	wire 		  De_pos;
	wire 		  De_neg;
	wire 		  Vs_pos;
		
	reg  [11:0]   De_vcnt     ;
	reg  [11:0]   De_hcnt     ;
	reg  [11:0]   De_hcnt_d1  ;
	reg  [11:0]   De_hcnt_d2  ;

	reg [23:0] colour_out;

	//-------------------------------
	wire [23:0]   Data_sel;

	//-------------------------------
	reg  [23:0]   Data_tmp/*synthesis syn_keep=1*/;

	//==============================================================================
	//Generate HS, VS, DE signals
	always@(posedge I_pxl_clk or negedge I_rst_n)
	begin
		if(!I_rst_n)
			V_cnt <= 12'd0;
		else     
			begin
				if((V_cnt >= (I_v_total-1'b1)) && (H_cnt >= (I_h_total-1'b1)))
					V_cnt <= 12'd0;
				else if(H_cnt >= (I_h_total-1'b1))
					V_cnt <=  V_cnt + 1'b1;
				else
					V_cnt <= V_cnt;
			end
	end

	//-------------------------------------------------------------    
	always @(posedge I_pxl_clk or negedge I_rst_n)
	begin
		if(!I_rst_n)
			H_cnt <=  12'd0; 
		else if(H_cnt >= (I_h_total-1'b1))
			H_cnt <=  12'd0 ; 
		else 
			H_cnt <=  H_cnt + 1'b1 ;           
	end

	//-------------------------------------------------------------
	assign  Pout_de_w = ((H_cnt>=(I_h_sync+I_h_bporch))&(H_cnt<=(I_h_sync+I_h_bporch+I_h_res-1'b1)))&
						((V_cnt>=(I_v_sync+I_v_bporch))&(V_cnt<=(I_v_sync+I_v_bporch+I_v_res-1'b1))) ;
	assign  Pout_hs_w =  ~((H_cnt>=12'd0) & (H_cnt<=(I_h_sync-1'b1))) ;
	assign  Pout_vs_w =  ~((V_cnt>=12'd0) & (V_cnt<=(I_v_sync-1'b1))) ;  

	//-------------------------------------------------------------
	always@(posedge I_pxl_clk or negedge I_rst_n)
	begin
		if(!I_rst_n)
			begin
				Pout_de_dn  <= {N{1'b0}};                          
				Pout_hs_dn  <= {N{1'b1}};
				Pout_vs_dn  <= {N{1'b1}}; 
			end
		else 
			begin
				Pout_de_dn  <= {Pout_de_dn[N-2:0],Pout_de_w};                          
				Pout_hs_dn  <= {Pout_hs_dn[N-2:0],Pout_hs_w};
				Pout_vs_dn  <= {Pout_vs_dn[N-2:0],Pout_vs_w}; 
			end
	end

	assign O_de = Pout_de_dn[4];

	always@(posedge I_pxl_clk or negedge I_rst_n)
	begin
		if(!I_rst_n)
			begin                        
				O_hs  <= 1'b1;
				O_vs  <= 1'b1; 
			end
		else 
			begin                         
				O_hs  <= I_hs_pol ? ~Pout_hs_dn[3] : Pout_hs_dn[3] ;
				O_vs  <= I_vs_pol ? ~Pout_vs_dn[3] : Pout_vs_dn[3] ;
			end
	end

	assign De_pos	= !Pout_de_dn[1] & Pout_de_dn[0]; //de rising edge
	assign De_neg	= Pout_de_dn[1] && !Pout_de_dn[0];//de falling edge
	assign Vs_pos	= !Pout_vs_dn[1] && Pout_vs_dn[0];//vs rising edge

	always @(posedge I_pxl_clk or negedge I_rst_n)
    begin
        if(!I_rst_n) begin
            De_hcnt <= 12'd0;
        end else if (De_pos == 1'b1) begin
            De_hcnt <= 12'd0;
        end else if (Pout_de_dn[1] == 1'b1) begin
            De_hcnt <= De_hcnt + 1'b1;
        end else begin
            De_hcnt <= De_hcnt;
		end
    end

	always @(posedge I_pxl_clk or negedge I_rst_n)
	begin
		if(!I_rst_n) begin
			De_vcnt <= 12'd0;
		end else if (Vs_pos == 1'b1) begin
			De_vcnt <= 12'd0;
		end else if (De_neg == 1'b1) begin 
			De_vcnt <= De_vcnt + 1'b1;
		end else begin
			De_vcnt <= De_vcnt;
		end
	end

	//---------------------------------------------------------------------------------------------
	//renderer
	//---------------------------------------------------------------------------------------------

	always @(posedge I_pxl_clk) begin
		if (Pout_de_dn[1]) begin
			if (De_hcnt[3:0] == 4'b1101) 
				H_px_cnt <= H_px_cnt + 1'b1;
		end else if (!Pout_de_dn[1]) 
			H_px_cnt <= 0;
		
		if (De_neg) begin
			V_line_step <= V_line_step + 1'b1;
			if (V_line_step == 3'd7)
				V_px_cnt <= V_px_cnt + 1'b1;
		end

		if (Vs_pos) begin
			V_px_cnt <= 0;
		end
		
		if (H_px_cnt >= 12'd90) begin
			if (De_hcnt == 12'd0)
				H_px_cnt <= 1'b0;
			else
				colour_out <= BLACK;
		end
		if (V_px_cnt >= 12'd90) begin
			if (De_vcnt == 12'd0) begin
				V_px_cnt <= 1'b0;
			end else
				colour_out <= BLACK;
		end
		if (H_px_cnt < 12'd90) begin
			pixel_address <= 12'd90*V_px_cnt + H_px_cnt;
			if (palette == 2'b00) begin
				case (pixel_colour)
					2'b00 : colour_out <= {bgcolour[23:16], bgcolour[15:8], bgcolour[7:0]};
					2'b01 : colour_out <= RED;
					2'b10 : colour_out <= GREEN;
					2'b11 : colour_out <= BLUE;
					default : colour_out <= BLACK;
				endcase  
			end else if (palette == 2'b01) begin
				case (pixel_colour)
					2'b00 : colour_out <= {(8'd255-bgcolour[23:16]), (8'd255-bgcolour[15:8]), (8'd255-bgcolour[7:0])};
					2'b01 : colour_out <= CYAN;
					2'b10 : colour_out <= MAGENTA;
					2'b11 : colour_out <= YELLOW;
					default : colour_out <= BLACK;
				endcase  
			end else if (palette == 2'b10) begin
				case (pixel_colour)
					2'b00 : colour_out <= BLACK;
					2'b01 : colour_out <= WHITE;
					2'b10 : colour_out <= WHITE;
					2'b11 : colour_out <= WHITE;
					default : colour_out <= BLACK;
				endcase  
			end else if (palette == 2'b11) begin
				case (pixel_colour)
					2'b00 : colour_out <= WHITE;
					2'b01 : colour_out <= BLACK;
					2'b10 : colour_out <= BLACK;
					2'b11 : colour_out <= BLACK;
					default : colour_out <= WHITE;
				endcase  
			end
		end 
	end

	//---------------------------------------------------------------------------------------------

	assign Data_sel = colour_out;

	always @(posedge I_pxl_clk or negedge I_rst_n)
	begin
		if(!I_rst_n) 
			Data_tmp <= 24'd0;
		else
			Data_tmp <= Data_sel;
	end

	assign O_data_r = Data_tmp[ 7: 0];
	assign O_data_g = Data_tmp[15: 8];
	assign O_data_b = Data_tmp[23:16];

endmodule       
              
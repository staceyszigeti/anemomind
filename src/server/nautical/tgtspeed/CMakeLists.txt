add_library(nautical_tgtspeed_TargetSpeedParam
            TargetSpeedParam.h
            TargetSpeedParam.cpp
           )
target_link_libraries(nautical_tgtspeed_TargetSpeedParam
                      math_Grid
                      nautical_tgtspeed_table
                      plot_gnuplot
                     )     

cxx_test(nautical_tgtspeed_TargetSpeedParamTest
         TargetSpeedParamTest.cpp
         nautical_tgtspeed_TargetSpeedParam
         gtest_main
        )

add_library(nautical_tgtspeed_TargetSpeedPoint
            TargetSpeedPoint.h
            TargetSpeedPoint.cpp
           )   
           
add_library(nautical_tgtspeed_table
            table.h
            table.cpp
           )               
target_link_libraries(nautical_tgtspeed_table
                      common_string
                     )                                     
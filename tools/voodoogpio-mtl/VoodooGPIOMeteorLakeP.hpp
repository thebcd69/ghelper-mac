//
//  VoodooGPIOMeteorLakeP.hpp
//  VoodooGPIO
//
//  Meteor Lake-P (INTC1083) / Arrow Lake-H (INTC105E) pin controller. Tables from Linux
//  drivers/pinctrl/intel/pinctrl-meteorlake.c (Andy Shevchenko, Intel), GPL-2.0.
//  Added 2026-11-17 for the ASUS ROG Zephyrus G16 GU605MY (Core Ultra 9 185H).
//

#ifndef VoodooGPIOMeteorLakeP_hpp
#define VoodooGPIOMeteorLakeP_hpp

#include "../VoodooGPIOIntel.hpp"

#define MTL_P_PAD_OWN       0x0b0
#define MTL_P_PADCFGLOCK    0x110
#define MTL_P_HOSTSW_OWN    0x140
#define MTL_P_GPI_IS        0x200
#define MTL_P_GPI_IE        0x210

#define MTL_P_GPP(r, s, e, g)         \
    {                                 \
        .reg_num = (r),               \
        .base = (s),                  \
        .size = ((e) - (s) + 1),      \
        .gpio_base = (g),             \
    }

#define MTL_P_COMMUNITY(b, s, e, g)               \
    {                                             \
        .barno = (b),                             \
        .padown_offset = MTL_P_PAD_OWN,           \
        .padcfglock_offset = MTL_P_PADCFGLOCK,    \
        .hostown_offset = MTL_P_HOSTSW_OWN,       \
        .is_offset = MTL_P_GPI_IS,                \
        .ie_offset = MTL_P_GPI_IE,                \
        .pin_base = (s),                          \
        .npins = ((e) - (s) + 1),                 \
        .gpps = (g),                              \
        .ngpps = ARRAY_SIZE(g),                   \
    }

static const struct pinctrl_pin_desc mtlp_pins[] = {
    /* CPU */
    PINCTRL_PIN(0, (char *)"PECI"),
    PINCTRL_PIN(1, (char *)"UFS_RESET_B"),
    PINCTRL_PIN(2, (char *)"VIDSOUT"),
    PINCTRL_PIN(3, (char *)"VIDSCK"),
    PINCTRL_PIN(4, (char *)"VIDALERT_B"),
    /* GPP_V */
    PINCTRL_PIN(5, (char *)"BATLOW_B"),
    PINCTRL_PIN(6, (char *)"AC_PRESENT"),
    PINCTRL_PIN(7, (char *)"SOC_WAKE_B"),
    PINCTRL_PIN(8, (char *)"PWRBTN_B"),
    PINCTRL_PIN(9, (char *)"SLP_S3_B"),
    PINCTRL_PIN(10, (char *)"SLP_S4_B"),
    PINCTRL_PIN(11, (char *)"SLP_A_B"),
    PINCTRL_PIN(12, (char *)"GPP_V_7"),
    PINCTRL_PIN(13, (char *)"SUSCLK"),
    PINCTRL_PIN(14, (char *)"SLP_WLAN_B"),
    PINCTRL_PIN(15, (char *)"SLP_S5_B"),
    PINCTRL_PIN(16, (char *)"LANPHYPC"),
    PINCTRL_PIN(17, (char *)"SLP_LAN_B"),
    PINCTRL_PIN(18, (char *)"GPP_V_13"),
    PINCTRL_PIN(19, (char *)"WAKE_B"),
    PINCTRL_PIN(20, (char *)"GPP_V_15"),
    PINCTRL_PIN(21, (char *)"GPP_V_16"),
    PINCTRL_PIN(22, (char *)"GPP_V_17"),
    PINCTRL_PIN(23, (char *)"GPP_V_18"),
    PINCTRL_PIN(24, (char *)"CATERR_B"),
    PINCTRL_PIN(25, (char *)"PROCHOT_B"),
    PINCTRL_PIN(26, (char *)"THERMTRIP_B"),
    PINCTRL_PIN(27, (char *)"DSI_DE_TE_2_GENLOCK_REF"),
    PINCTRL_PIN(28, (char *)"DSI_DE_TE_1_DISP_UTILS"),
    /* GPP_C */
    PINCTRL_PIN(29, (char *)"SMBCLK"),
    PINCTRL_PIN(30, (char *)"SMBDATA"),
    PINCTRL_PIN(31, (char *)"SMBALERT_B"),
    PINCTRL_PIN(32, (char *)"SML0CLK"),
    PINCTRL_PIN(33, (char *)"SML0DATA"),
    PINCTRL_PIN(34, (char *)"GPP_C_5"),
    PINCTRL_PIN(35, (char *)"GPP_C_6"),
    PINCTRL_PIN(36, (char *)"GPP_C_7"),
    PINCTRL_PIN(37, (char *)"GPP_C_8"),
    PINCTRL_PIN(38, (char *)"GPP_C_9"),
    PINCTRL_PIN(39, (char *)"GPP_C_10"),
    PINCTRL_PIN(40, (char *)"GPP_C_11"),
    PINCTRL_PIN(41, (char *)"GPP_C_12"),
    PINCTRL_PIN(42, (char *)"GPP_C_13"),
    PINCTRL_PIN(43, (char *)"GPP_C_14"),
    PINCTRL_PIN(44, (char *)"GPP_C_15"),
    PINCTRL_PIN(45, (char *)"GPP_C_16"),
    PINCTRL_PIN(46, (char *)"GPP_C_17"),
    PINCTRL_PIN(47, (char *)"GPP_C_18"),
    PINCTRL_PIN(48, (char *)"GPP_C_19"),
    PINCTRL_PIN(49, (char *)"GPP_C_20"),
    PINCTRL_PIN(50, (char *)"GPP_C_21"),
    PINCTRL_PIN(51, (char *)"GPP_C_22"),
    PINCTRL_PIN(52, (char *)"GPP_C_23"),
    /* GPP_A */
    PINCTRL_PIN(53, (char *)"ESPI_IO_0"),
    PINCTRL_PIN(54, (char *)"ESPI_IO_1"),
    PINCTRL_PIN(55, (char *)"ESPI_IO_2"),
    PINCTRL_PIN(56, (char *)"ESPI_IO_3"),
    PINCTRL_PIN(57, (char *)"ESPI_CS0_B"),
    PINCTRL_PIN(58, (char *)"ESPI_CLK"),
    PINCTRL_PIN(59, (char *)"ESPI_RESET_B"),
    PINCTRL_PIN(60, (char *)"GPP_A_7"),
    PINCTRL_PIN(61, (char *)"GPP_A_8"),
    PINCTRL_PIN(62, (char *)"GPP_A_9"),
    PINCTRL_PIN(63, (char *)"GPP_A_10"),
    PINCTRL_PIN(64, (char *)"GPP_A_11"),
    PINCTRL_PIN(65, (char *)"GPP_A_12"),
    PINCTRL_PIN(66, (char *)"ESPI_CS1_B"),
    PINCTRL_PIN(67, (char *)"ESPI_CS2_B"),
    PINCTRL_PIN(68, (char *)"ESPI_CS3_B"),
    PINCTRL_PIN(69, (char *)"ESPI_ALERT0_B"),
    PINCTRL_PIN(70, (char *)"ESPI_ALERT1_B"),
    PINCTRL_PIN(71, (char *)"ESPI_ALERT2_B"),
    PINCTRL_PIN(72, (char *)"ESPI_ALERT3_B"),
    PINCTRL_PIN(73, (char *)"GPP_A_20"),
    PINCTRL_PIN(74, (char *)"GPP_A_21"),
    PINCTRL_PIN(75, (char *)"GPP_A_22"),
    PINCTRL_PIN(76, (char *)"GPP_A_23"),
    PINCTRL_PIN(77, (char *)"ESPI_CLK_LOOPBK"),
    /* GPP_E */
    PINCTRL_PIN(78, (char *)"GPP_E_0"),
    PINCTRL_PIN(79, (char *)"GPP_E_1"),
    PINCTRL_PIN(80, (char *)"GPP_E_2"),
    PINCTRL_PIN(81, (char *)"GPP_E_3"),
    PINCTRL_PIN(82, (char *)"GPP_E_4"),
    PINCTRL_PIN(83, (char *)"GPP_E_5"),
    PINCTRL_PIN(84, (char *)"GPP_E_6"),
    PINCTRL_PIN(85, (char *)"GPP_E_7"),
    PINCTRL_PIN(86, (char *)"GPP_E_8"),
    PINCTRL_PIN(87, (char *)"GPP_E_9"),
    PINCTRL_PIN(88, (char *)"GPP_E_10"),
    PINCTRL_PIN(89, (char *)"GPP_E_11"),
    PINCTRL_PIN(90, (char *)"GPP_E_12"),
    PINCTRL_PIN(91, (char *)"GPP_E_13"),
    PINCTRL_PIN(92, (char *)"GPP_E_14"),
    PINCTRL_PIN(93, (char *)"SLP_DRAM_B"),
    PINCTRL_PIN(94, (char *)"GPP_E_16"),
    PINCTRL_PIN(95, (char *)"GPP_E_17"),
    PINCTRL_PIN(96, (char *)"GPP_E_18"),
    PINCTRL_PIN(97, (char *)"GPP_E_19"),
    PINCTRL_PIN(98, (char *)"GPP_E_20"),
    PINCTRL_PIN(99, (char *)"GPP_E_21"),
    PINCTRL_PIN(100, (char *)"DNX_FORCE_RELOAD"),
    PINCTRL_PIN(101, (char *)"GPP_E_23"),
    PINCTRL_PIN(102, (char *)"THC0_GSPI0_CLK_LOOPBK"),
    /* GPP_H */
    PINCTRL_PIN(103, (char *)"GPP_H_0"),
    PINCTRL_PIN(104, (char *)"GPP_H_1"),
    PINCTRL_PIN(105, (char *)"GPP_H_2"),
    PINCTRL_PIN(106, (char *)"GPP_H_3"),
    PINCTRL_PIN(107, (char *)"GPP_H_4"),
    PINCTRL_PIN(108, (char *)"GPP_H_5"),
    PINCTRL_PIN(109, (char *)"GPP_H_6"),
    PINCTRL_PIN(110, (char *)"GPP_H_7"),
    PINCTRL_PIN(111, (char *)"GPP_H_8"),
    PINCTRL_PIN(112, (char *)"GPP_H_9"),
    PINCTRL_PIN(113, (char *)"GPP_H_10"),
    PINCTRL_PIN(114, (char *)"GPP_H_11"),
    PINCTRL_PIN(115, (char *)"GPP_H_12"),
    PINCTRL_PIN(116, (char *)"CPU_C10_GATE_B"),
    PINCTRL_PIN(117, (char *)"GPP_H_14"),
    PINCTRL_PIN(118, (char *)"GPP_H_15"),
    PINCTRL_PIN(119, (char *)"GPP_H_16"),
    PINCTRL_PIN(120, (char *)"GPP_H_17"),
    PINCTRL_PIN(121, (char *)"GPP_H_18"),
    PINCTRL_PIN(122, (char *)"GPP_H_19"),
    PINCTRL_PIN(123, (char *)"GPP_H_20"),
    PINCTRL_PIN(124, (char *)"GPP_H_21"),
    PINCTRL_PIN(125, (char *)"GPP_H_22"),
    PINCTRL_PIN(126, (char *)"GPP_H_23"),
    PINCTRL_PIN(127, (char *)"LPI3C1_CLK_LOOPBK"),
    PINCTRL_PIN(128, (char *)"I3C0_CLK_LOOPBK"),
    /* GPP_F */
    PINCTRL_PIN(129, (char *)"CNV_BRI_DT"),
    PINCTRL_PIN(130, (char *)"CNV_BRI_RSP"),
    PINCTRL_PIN(131, (char *)"CNV_RGI_DT"),
    PINCTRL_PIN(132, (char *)"CNV_RGI_RSP"),
    PINCTRL_PIN(133, (char *)"CNV_RF_RESET_B"),
    PINCTRL_PIN(134, (char *)"CRF_CLKREQ"),
    PINCTRL_PIN(135, (char *)"GPP_F_6"),
    PINCTRL_PIN(136, (char *)"FUSA_DIAGTEST_EN"),
    PINCTRL_PIN(137, (char *)"FUSA_DIAGTEST_MODE"),
    PINCTRL_PIN(138, (char *)"BOOTMPC"),
    PINCTRL_PIN(139, (char *)"GPP_F_10"),
    PINCTRL_PIN(140, (char *)"GPP_F_11"),
    PINCTRL_PIN(141, (char *)"GSXDOUT"),
    PINCTRL_PIN(142, (char *)"GSXSLOAD"),
    PINCTRL_PIN(143, (char *)"GSXDIN"),
    PINCTRL_PIN(144, (char *)"GSXSRESETB"),
    PINCTRL_PIN(145, (char *)"GSXCLK"),
    PINCTRL_PIN(146, (char *)"GMII_MDC_0"),
    PINCTRL_PIN(147, (char *)"GMII_MDIO_0"),
    PINCTRL_PIN(148, (char *)"GPP_F_19"),
    PINCTRL_PIN(149, (char *)"GPP_F_20"),
    PINCTRL_PIN(150, (char *)"GPP_F_21"),
    PINCTRL_PIN(151, (char *)"GPP_F_22"),
    PINCTRL_PIN(152, (char *)"GPP_F_23"),
    PINCTRL_PIN(153, (char *)"THC1_GSPI1_CLK_LOOPBK"),
    PINCTRL_PIN(154, (char *)"GSPI0A_CLK_LOOPBK"),
    /* SPI0 */
    PINCTRL_PIN(155, (char *)"SPI0_IO_2"),
    PINCTRL_PIN(156, (char *)"SPI0_IO_3"),
    PINCTRL_PIN(157, (char *)"SPI0_MOSI_IO_0"),
    PINCTRL_PIN(158, (char *)"SPI0_MISO_IO_1"),
    PINCTRL_PIN(159, (char *)"SPI0_TPM_CS_B"),
    PINCTRL_PIN(160, (char *)"SPI0_FLASH_0_CS_B"),
    PINCTRL_PIN(161, (char *)"SPI0_FLASH_1_CS_B"),
    PINCTRL_PIN(162, (char *)"SPI0_CLK"),
    PINCTRL_PIN(163, (char *)"L_BKLTEN"),
    PINCTRL_PIN(164, (char *)"L_BKLTCTL"),
    PINCTRL_PIN(165, (char *)"L_VDDEN"),
    PINCTRL_PIN(166, (char *)"SYS_PWROK"),
    PINCTRL_PIN(167, (char *)"SYS_RESET_B"),
    PINCTRL_PIN(168, (char *)"MLK_RST_B"),
    PINCTRL_PIN(169, (char *)"SPI0_CLK_LOOPBK"),
    /* vGPIO_3 */
    PINCTRL_PIN(170, (char *)"ESPI_USB_OCB_0"),
    PINCTRL_PIN(171, (char *)"ESPI_USB_OCB_1"),
    PINCTRL_PIN(172, (char *)"ESPI_USB_OCB_2"),
    PINCTRL_PIN(173, (char *)"ESPI_USB_OCB_3"),
    PINCTRL_PIN(174, (char *)"USB_CPU_OCB_0"),
    PINCTRL_PIN(175, (char *)"USB_CPU_OCB_1"),
    PINCTRL_PIN(176, (char *)"USB_CPU_OCB_2"),
    PINCTRL_PIN(177, (char *)"USB_CPU_OCB_3"),
    PINCTRL_PIN(178, (char *)"TS0_IN_INT"),
    PINCTRL_PIN(179, (char *)"TS1_IN_INT"),
    PINCTRL_PIN(180, (char *)"THC0_WOT_INT"),
    PINCTRL_PIN(181, (char *)"THC1_WOT_INT"),
    PINCTRL_PIN(182, (char *)"THC0_WHC_INT"),
    PINCTRL_PIN(183, (char *)"THC1_WHC_INT"),
    /* GPP_S */
    PINCTRL_PIN(184, (char *)"GPP_S_0"),
    PINCTRL_PIN(185, (char *)"GPP_S_1"),
    PINCTRL_PIN(186, (char *)"GPP_S_2"),
    PINCTRL_PIN(187, (char *)"GPP_S_3"),
    PINCTRL_PIN(188, (char *)"GPP_S_4"),
    PINCTRL_PIN(189, (char *)"GPP_S_5"),
    PINCTRL_PIN(190, (char *)"GPP_S_6"),
    PINCTRL_PIN(191, (char *)"GPP_S_7"),
    /* JTAG */
    PINCTRL_PIN(192, (char *)"JTAG_MBPB0"),
    PINCTRL_PIN(193, (char *)"JTAG_MBPB1"),
    PINCTRL_PIN(194, (char *)"JTAG_MBPB2"),
    PINCTRL_PIN(195, (char *)"JTAG_MBPB3"),
    PINCTRL_PIN(196, (char *)"JTAG_TDO"),
    PINCTRL_PIN(197, (char *)"PRDY_B"),
    PINCTRL_PIN(198, (char *)"PREQ_B"),
    PINCTRL_PIN(199, (char *)"JTAG_TDI"),
    PINCTRL_PIN(200, (char *)"JTAG_TMS"),
    PINCTRL_PIN(201, (char *)"JTAG_TCK"),
    PINCTRL_PIN(202, (char *)"DBG_PMODE"),
    PINCTRL_PIN(203, (char *)"JTAG_TRST_B"),
    /* GPP_B */
    PINCTRL_PIN(204, (char *)"ADM_VID_0"),
    PINCTRL_PIN(205, (char *)"ADM_VID_1"),
    PINCTRL_PIN(206, (char *)"GPP_B_2"),
    PINCTRL_PIN(207, (char *)"GPP_B_3"),
    PINCTRL_PIN(208, (char *)"GPP_B_4"),
    PINCTRL_PIN(209, (char *)"GPP_B_5"),
    PINCTRL_PIN(210, (char *)"GPP_B_6"),
    PINCTRL_PIN(211, (char *)"GPP_B_7"),
    PINCTRL_PIN(212, (char *)"GPP_B_8"),
    PINCTRL_PIN(213, (char *)"GPP_B_9"),
    PINCTRL_PIN(214, (char *)"GPP_B_10"),
    PINCTRL_PIN(215, (char *)"GPP_B_11"),
    PINCTRL_PIN(216, (char *)"SLP_S0_B"),
    PINCTRL_PIN(217, (char *)"PLTRST_B"),
    PINCTRL_PIN(218, (char *)"GPP_B_14"),
    PINCTRL_PIN(219, (char *)"GPP_B_15"),
    PINCTRL_PIN(220, (char *)"GPP_B_16"),
    PINCTRL_PIN(221, (char *)"GPP_B_17"),
    PINCTRL_PIN(222, (char *)"GPP_B_18"),
    PINCTRL_PIN(223, (char *)"GPP_B_19"),
    PINCTRL_PIN(224, (char *)"GPP_B_20"),
    PINCTRL_PIN(225, (char *)"GPP_B_21"),
    PINCTRL_PIN(226, (char *)"GPP_B_22"),
    PINCTRL_PIN(227, (char *)"GPP_B_23"),
    PINCTRL_PIN(228, (char *)"ISH_I3C0_CLK_LOOPBK"),
    /* GPP_D */
    PINCTRL_PIN(229, (char *)"GPP_D_0"),
    PINCTRL_PIN(230, (char *)"GPP_D_1"),
    PINCTRL_PIN(231, (char *)"GPP_D_2"),
    PINCTRL_PIN(232, (char *)"GPP_D_3"),
    PINCTRL_PIN(233, (char *)"GPP_D_4"),
    PINCTRL_PIN(234, (char *)"GPP_D_5"),
    PINCTRL_PIN(235, (char *)"GPP_D_6"),
    PINCTRL_PIN(236, (char *)"GPP_D_7"),
    PINCTRL_PIN(237, (char *)"GPP_D_8"),
    PINCTRL_PIN(238, (char *)"GPP_D_9"),
    PINCTRL_PIN(239, (char *)"HDA_BCLK"),
    PINCTRL_PIN(240, (char *)"HDA_SYNC"),
    PINCTRL_PIN(241, (char *)"HDA_SDO"),
    PINCTRL_PIN(242, (char *)"HDA_SDI_0"),
    PINCTRL_PIN(243, (char *)"GPP_D_14"),
    PINCTRL_PIN(244, (char *)"GPP_D_15"),
    PINCTRL_PIN(245, (char *)"GPP_D_16"),
    PINCTRL_PIN(246, (char *)"HDA_RST_B"),
    PINCTRL_PIN(247, (char *)"GPP_D_18"),
    PINCTRL_PIN(248, (char *)"GPP_D_19"),
    PINCTRL_PIN(249, (char *)"GPP_D_20"),
    PINCTRL_PIN(250, (char *)"UFS_REFCLK"),
    PINCTRL_PIN(251, (char *)"BPKI3C_SDA"),
    PINCTRL_PIN(252, (char *)"BPKI3C_SCL"),
    PINCTRL_PIN(253, (char *)"BOOTHALT_B"),
    /* vGPIO */
    PINCTRL_PIN(254, (char *)"CNV_BTEN"),
    PINCTRL_PIN(255, (char *)"CNV_BT_HOST_WAKEB"),
    PINCTRL_PIN(256, (char *)"CNV_BT_IF_SELECT"),
    PINCTRL_PIN(257, (char *)"vCNV_BT_UART_TXD"),
    PINCTRL_PIN(258, (char *)"vCNV_BT_UART_RXD"),
    PINCTRL_PIN(259, (char *)"vCNV_BT_UART_CTS_B"),
    PINCTRL_PIN(260, (char *)"vCNV_BT_UART_RTS_B"),
    PINCTRL_PIN(261, (char *)"vCNV_MFUART1_TXD"),
    PINCTRL_PIN(262, (char *)"vCNV_MFUART1_RXD"),
    PINCTRL_PIN(263, (char *)"vCNV_MFUART1_CTS_B"),
    PINCTRL_PIN(264, (char *)"vCNV_MFUART1_RTS_B"),
    PINCTRL_PIN(265, (char *)"vUART0_TXD"),
    PINCTRL_PIN(266, (char *)"vUART0_RXD"),
    PINCTRL_PIN(267, (char *)"vUART0_CTS_B"),
    PINCTRL_PIN(268, (char *)"vUART0_RTS_B"),
    PINCTRL_PIN(269, (char *)"vISH_UART0_TXD"),
    PINCTRL_PIN(270, (char *)"vISH_UART0_RXD"),
    PINCTRL_PIN(271, (char *)"vISH_UART0_CTS_B"),
    PINCTRL_PIN(272, (char *)"vISH_UART0_RTS_B"),
    PINCTRL_PIN(273, (char *)"vCNV_BT_I2S_BCLK"),
    PINCTRL_PIN(274, (char *)"vCNV_BT_I2S_WS_SYNC"),
    PINCTRL_PIN(275, (char *)"vCNV_BT_I2S_SDO"),
    PINCTRL_PIN(276, (char *)"vCNV_BT_I2S_SDI"),
    PINCTRL_PIN(277, (char *)"vI2S2_SCLK"),
    PINCTRL_PIN(278, (char *)"vI2S2_SFRM"),
    PINCTRL_PIN(279, (char *)"vI2S2_TXD"),
    PINCTRL_PIN(280, (char *)"vI2S2_RXD"),
    PINCTRL_PIN(281, (char *)"vCNV_BT_I2S_BCLK_2"),
    PINCTRL_PIN(282, (char *)"vCNV_BT_I2S_WS_SYNC_2"),
    PINCTRL_PIN(283, (char *)"vCNV_BT_I2S_SDO_2"),
    PINCTRL_PIN(284, (char *)"vCNV_BT_I2S_SDI_2"),
    PINCTRL_PIN(285, (char *)"vI2S2_SCLK_2"),
    PINCTRL_PIN(286, (char *)"vI2S2_SFRM_2"),
    PINCTRL_PIN(287, (char *)"vI2S2_TXD_2"),
    PINCTRL_PIN(288, (char *)"vI2S2_RXD_2"),
};

static struct intel_padgroup mtlp_community0_gpps[] = {
    MTL_P_GPP(0, 0, 4, 0),		/* CPU */
    MTL_P_GPP(1, 5, 28, 32),	/* GPP_V */
    MTL_P_GPP(2, 29, 52, 64),	/* GPP_C */
};

static struct intel_padgroup mtlp_community1_gpps[] = {
    MTL_P_GPP(0, 53, 77, 96),	/* GPP_A */
    MTL_P_GPP(1, 78, 102, 128),	/* GPP_E */
};

static struct intel_padgroup mtlp_community3_gpps[] = {
    MTL_P_GPP(0, 103, 128, 160),	/* GPP_H */
    MTL_P_GPP(1, 129, 154, 192),	/* GPP_F */
    MTL_P_GPP(2, 155, 169, 224),	/* SPI0 */
    MTL_P_GPP(3, 170, 183, 256),	/* vGPIO_3 */
};

static struct intel_padgroup mtlp_community4_gpps[] = {
    MTL_P_GPP(0, 184, 191, 288),	/* GPP_S */
    MTL_P_GPP(1, 192, 203, 320),	/* JTAG */
};

static struct intel_padgroup mtlp_community5_gpps[] = {
    MTL_P_GPP(0, 204, 228, 352),	/* GPP_B */
    MTL_P_GPP(1, 229, 253, 384),	/* GPP_D */
    MTL_P_GPP(2, 254, 285, 416),	/* vGPIO_0 */
    MTL_P_GPP(3, 286, 288, 448),	/* vGPIO_1 */
};

static struct intel_community mtlp_communities[] = {
    MTL_P_COMMUNITY(0, 0, 52, mtlp_community0_gpps),
    MTL_P_COMMUNITY(1, 53, 102, mtlp_community1_gpps),
    MTL_P_COMMUNITY(2, 103, 183, mtlp_community3_gpps),
    MTL_P_COMMUNITY(3, 184, 203, mtlp_community4_gpps),
    MTL_P_COMMUNITY(4, 204, 288, mtlp_community5_gpps),
};

class VoodooGPIOMeteorLakeP : public VoodooGPIOIntel {
    OSDeclareDefaultStructors(VoodooGPIOMeteorLakeP);

    bool start(IOService *provider) override;
};

#endif /* VoodooGPIOMeteorLakeP_hpp */

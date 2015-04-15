<?xml version="1.0" encoding="UTF-8"?>

<xsl:transform version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:qct="http://www.quickcamteam.net"
>

<xsl:output method="text" />

<xsl:strip-space elements="qct:config qct:constants" />


<!-- Only process the root 'config' element -->
<xsl:template match="/">
	<xsl:apply-templates select="qct:config/qct:meta" />

/* Start constants */
	<xsl:apply-templates select="qct:config/qct:constants" />

/* End constants */
</xsl:template>


<xsl:template match="qct:meta">
/*
 * UVC dynamic control mapping header file
 *
 * This file has been dynamically generated using the include.xsl XSLT
 * transform stylesheet from a dynamic control mapping configuration file.
 *<xsl:apply-templates select="qct:version|qct:author|qct:contact|qct:revision|qct:copyright" />
 */
</xsl:template>


<xsl:template match="qct:version">
 * Version: <xsl:apply-templates />
</xsl:template>


<xsl:template match="qct:author">
 * Author: <xsl:apply-templates />
</xsl:template>


<xsl:template match="qct:contact">
 * Contact: <xsl:apply-templates />
</xsl:template>


<xsl:template match="qct:revision">
 * Revision: <xsl:apply-templates />
</xsl:template>


<xsl:template match="qct:copyright">
 *
 * <xsl:apply-templates />
</xsl:template>


<xsl:template match="qct:config">
	<xsl:apply-templates select="qct:constants" />
</xsl:template>


<xsl:template match="qct:constants">
	<xsl:apply-templates />
</xsl:template>


<xsl:template match="qct:constant">
#ifndef <xsl:value-of select="qct:id" />
	<xsl:choose space="preserve">
		<!-- Integer type constant -->
		<xsl:when test="@type='integer'">
#define <xsl:value-of select="qct:id" /><xsl:text> </xsl:text><xsl:value-of select="qct:value" />
		</xsl:when>

		<!-- GUID type constant -->
		<xsl:when test="@type='guid'">
#define <xsl:value-of select="qct:id" /> { \
	0x<xsl:value-of select="substring(qct:value,7,2)" />, 0x<xsl:value-of select="substring(qct:value,5,2)" />, 0x<xsl:value-of select="substring(qct:value,3,2)" />, 0x<xsl:value-of select="substring(qct:value,1,2)" />, 0x<xsl:value-of select="substring(qct:value,12,2)" />, 0x<xsl:value-of select="substring(qct:value,10,2)" />, 0x<xsl:value-of select="substring(qct:value,17,2)" />, 0x<xsl:value-of select="substring(qct:value,15,2)" />, \
	0x<xsl:value-of select="substring(qct:value,20,2)" />, 0x<xsl:value-of select="substring(qct:value,22,2)" />, 0x<xsl:value-of select="substring(qct:value,25,2)" />, 0x<xsl:value-of select="substring(qct:value,27,2)" />, 0x<xsl:value-of select="substring(qct:value,29,2)" />, 0x<xsl:value-of select="substring(qct:value,31,2)" />, 0x<xsl:value-of select="substring(qct:value,33,2)" />, 0x<xsl:value-of select="substring(qct:value,35,2)" />  \
}
		</xsl:when>

		<!-- Unknown constant type -->
		<xsl:otherwise>
/* Unknown constant type '<xsl:value-of select="@type" />' encountered */
/* #define <xsl:value-of select="qct:id" /><xsl:text> </xsl:text><xsl:value-of select="qct:value" /> */
		</xsl:otherwise>
	</xsl:choose>
#endif
</xsl:template>


</xsl:transform>

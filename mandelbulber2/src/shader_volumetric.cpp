/**
 * Mandelbulber v2, a 3D fractal generator       ,=#MKNmMMKmmßMNWy,
 *                                             ,B" ]L,,p%%%,,,§;, "K
 * Copyright (C) 2018 Mandelbulber Team        §R-==%w["'~5]m%=L.=~5N
 *                                        ,=mm=§M ]=4 yJKA"/-Nsaj  "Bw,==,,
 * This file is part of Mandelbulber.    §R.r= jw",M  Km .mM  FW ",§=ß., ,TN
 *                                     ,4R =%["w[N=7]J '"5=],""]]M,w,-; T=]M
 * Mandelbulber is free software:     §R.ß~-Q/M=,=5"v"]=Qf,'§"M= =,M.§ Rz]M"Kw
 * you can redistribute it and/or     §w "xDY.J ' -"m=====WeC=\ ""%""y=%"]"" §
 * modify it under the terms of the    "§M=M =D=4"N #"%==A%p M§ M6  R' #"=~.4M
 * GNU General Public License as        §W =, ][T"]C  §  § '§ e===~ U  !§[Z ]N
 * published by the                    4M",,Jm=,"=e~  §  §  j]]""N  BmM"py=ßM
 * Free Software Foundation,          ]§ T,M=& 'YmMMpM9MMM%=w=,,=MT]M m§;'§,
 * either version 3 of the License,    TWw [.j"5=~N[=§%=%W,T ]R,"=="Y[LFT ]N
 * or (at your option)                   TW=,-#"%=;[  =Q:["V""  ],,M.m == ]N
 * any later version.                      J§"mr"] ,=,," =="""J]= M"M"]==ß"
 *                                          §= "=C=4 §"eM "=B:m|4"]#F,§~
 * Mandelbulber is distributed in            "9w=,,]w em%wJ '"~" ,=,,ß"
 * the hope that it will be useful,                 . "K=  ,=RMMMßM"""
 * but WITHOUT ANY WARRANTY;                            .'''
 * without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with Mandelbulber. If not, see <http://www.gnu.org/licenses/>.
 *
 * ###########################################################################
 *
 * Authors: Krzysztof Marczak (buddhi1980@gmail.com)
 *
 * cRenderWorker::VolumetricShader method - calculates volumetric shaders
 */

#include "compute_fractal.hpp"
#include "fractparams.hpp"
#include "render_data.hpp"
#include "render_worker.hpp"

sRGBAfloat cRenderWorker::VolumetricShader(
	const sShaderInputData &input, sRGBAfloat oldPixel, sRGBAfloat *opacityOut) const
{
	sRGBAfloat output;
	float totalOpacity = 0.0;

	output.R = oldPixel.R;
	output.G = oldPixel.G;
	output.B = oldPixel.B;
	output.A = oldPixel.A;

	// volumetric fog init
	double colourThresh = params->volFogColour1Distance;
	double colourThresh2 = params->volFogColour2Distance;
	double fogReduce = params->volFogDistanceFactor;
	float fogIntensity = params->volFogDensity;

	// visible lights init
	int numberOfLights = data->lights.GetNumberOfLights();
	if (numberOfLights < 4) numberOfLights = 4;

	// glow init
	float glow = input.stepCount * params->glowIntensity / 512.0f * float(params->DEFactor);
	float glowN = 1.0f - glow;
	if (glowN < 0.0f) glowN = 0.0f;
	float glowR = (params->glowColor1.R * glowN + params->glowColor2.R * glow) / 65536.0f;
	float glowG = (params->glowColor1.G * glowN + params->glowColor2.G * glow) / 65536.0f;
	float glowB = (params->glowColor1.B * glowN + params->glowColor2.B * glow) / 65536.0f;

	double totalStep = 0.0;

	// qDebug() << "Start volumetric shader &&&&&&&&&&&&&&&&&&&&";

	sShaderInputData input2 = input;
	for (int index = input.stepCount - 1; index > 0; index--)
	{
		double step = input.stepBuff[index].step;
		double distance = input.stepBuff[index].distance;
		CVector3 point = input.stepBuff[index].point;
		totalStep += step;

		input2.point = point;
		input2.distThresh = input.stepBuff[index].distThresh;

		// qDebug() << "i" << index << "dist" << distance << "iters" << input.stepBuff[index].iters <<
		// "distThresh" << input2.distThresh << "step" << step << "point" << point.Debug();

		if (totalStep < 1e-10 * CalcDistThresh(point)) // if two steps are the same
		{
			continue;
		}
		step = totalStep;
		totalStep = 0.0;

		//------------------- glow
		if (params->glowEnabled && input.stepCount > 0)
		{
			float glowOpacity = glow / input.stepCount;
			if (glowOpacity > 1.0f) glowOpacity = 1.0f;
			output.R = glowOpacity * glowR + (1.0f - glowOpacity) * output.R;
			output.G = glowOpacity * glowG + (1.0f - glowOpacity) * output.G;
			output.B = glowOpacity * glowB + (1.0f - glowOpacity) * output.B;
			output.A += glowOpacity;
		}
		// qDebug() << "step" << step;
		//------------------ visible light
		if (data->lights.IsAnyLightEnabled() && params->auxLightVisibility > 0)
		{
			for (int i = 0; i < numberOfLights; ++i)
			{
				const sLight *light = data->lights.GetLight(i);
				if (light->enabled && light->intensity > 0)
				{
					double lastMiniSteps = -1.0;
					double miniStep;

					for (double miniSteps = 0.0; miniSteps < step; miniSteps += miniStep)
					{
						CVector3 lightDistVect = point - input.viewVector * miniSteps - light->position;
						double lightDist = lightDistVect.Length();
						double lightSize = sqrt(light->intensity) * params->auxLightVisibilitySize;

						double distToLightSurface = lightDist - lightSize;
						if (distToLightSurface < 0.0) distToLightSurface = 0.0;

						miniStep = 0.1 * (distToLightSurface + 0.1 * distToLightSurface);
						if (miniStep > step - miniSteps) miniStep = step - miniSteps;
						if (miniStep < step * 0.001) miniStep = step * 0.001;

						double r2 = lightDist / lightSize;
						double bellFunction = 1.0 / (1.0 + pow(r2, 4.0));
						double lightDensity = miniStep * bellFunction * params->auxLightVisibility / lightSize;

						output.R += lightDensity * light->colour.R / 65536.0;
						output.G += lightDensity * light->colour.G / 65536.0;
						output.B += lightDensity * light->colour.B / 65536.0;
						output.A += lightDensity;

						if (miniSteps == lastMiniSteps)
						{
							// qWarning() << "Dead computation\n"
							//		<< "\pointN:" << (point - input.viewVector * miniSteps).Debug();
							break;
						}
						lastMiniSteps = miniSteps;
					}
				}
			}
		}

		// fake lights (orbit trap)
		if (params->fakeLightsEnabled)
		{
			sFractalIn fractIn(point, params->minN, params->N, params->common, -1, false);
			sFractalOut fractOut;
			Compute<fractal::calcModeOrbitTrap>(*fractal, fractIn, &fractOut);
			double r = fractOut.orbitTrapR;
			r = sqrt(1.0 / (r + 1.0e-30));
			double fakeLight = 1.0
												 / (pow(r, 10.0 / params->fakeLightsVisibilitySize)
															 * pow(10.0, 10.0 / params->fakeLightsVisibilitySize)
														 + 1e-100);
			output.R +=
				fakeLight * step * params->fakeLightsVisibility * params->fakeLightsColor.R / 65536.0f;
			output.G +=
				fakeLight * step * params->fakeLightsVisibility * params->fakeLightsColor.G / 65536.0f;
			output.B +=
				fakeLight * step * params->fakeLightsVisibility * params->fakeLightsColor.B / 65536.0f;
			output.A += fakeLight * step * params->fakeLightsVisibility;
		}

		//---------------------- volumetric lights with shadows in fog

		for (int i = 0; i < 5; i++)
		{
			if (i == 0 && params->volumetricLightEnabled[0] && params->mainLightEnable)
			{
				sRGBAfloat shadowOutputTemp = MainShadow(input2);
				output.R += shadowOutputTemp.R * step * params->volumetricLightIntensity[0]
										* params->mainLightColour.R / 65536.0;
				output.G += shadowOutputTemp.G * step * params->volumetricLightIntensity[0]
										* params->mainLightColour.G / 65536.0;
				output.B += shadowOutputTemp.B * step * params->volumetricLightIntensity[0]
										* params->mainLightColour.B / 65536.0;
				output.A += (shadowOutputTemp.R + shadowOutputTemp.G + shadowOutputTemp.B) / 3.0 * step
										* params->volumetricLightIntensity[0];
			}
			if (data->lights.IsAnyLightEnabled() && i > 0)
			{
				const sLight *light = data->lights.GetLight(i - 1);
				if (light->enabled && params->volumetricLightEnabled[i])
				{
					CVector3 lightVectorTemp = light->position - point;
					double distanceLight = lightVectorTemp.Length();
					double distanceLight2 = distanceLight * distanceLight;
					lightVectorTemp.Normalize();
					double lightShadow = AuxShadow(input2, distanceLight, lightVectorTemp, light->intensity);
					output.R += lightShadow * light->colour.R / 65536.0 * params->volumetricLightIntensity[i]
											* step / distanceLight2;
					output.G += lightShadow * light->colour.G / 65536.0 * params->volumetricLightIntensity[i]
											* step / distanceLight2;
					output.B += lightShadow * light->colour.B / 65536.0 * params->volumetricLightIntensity[i]
											* step / distanceLight2;
					output.A += lightShadow * params->volumetricLightIntensity[i] * step / distanceLight2;
				}
			}
		}

		//----------------------- basic fog
		if (params->fogEnabled)
		{
			float fogDensity = step / params->fogVisibility;
			if (fogDensity > 1.0f) fogDensity = 1.0f;
			output.R = fogDensity * params->fogColor.R / 65536.0f + (1.0f - fogDensity) * output.R;
			output.G = fogDensity * params->fogColor.G / 65536.0f + (1.0f - fogDensity) * output.G;
			output.B = fogDensity * params->fogColor.B / 65536.0f + (1.0f - fogDensity) * output.B;
			totalOpacity = fogDensity + (1.0f - fogDensity) * totalOpacity;
			output.A = fogDensity + (1.0f - fogDensity) * output.A;
		}

		//-------------------- volumetric fog
		if (fogIntensity > 0.0f && params->volFogEnabled)
		{
			double distanceShifted = fabs(distance - params->volFogDistanceFromSurface)
															 + 0.1 * params->volFogDistanceFromSurface;
			float densityTemp =
				step * fogReduce / (distanceShifted * distanceShifted + fogReduce * fogReduce);

			float k = distanceShifted / colourThresh;
			if (k > 1) k = 1.0f;
			float kn = 1.0f - k;
			float fogTempR = params->volFogColour1.R * kn + params->volFogColour2.R * k;
			float fogTempG = params->volFogColour1.G * kn + params->volFogColour2.G * k;
			float fogTempB = params->volFogColour1.B * kn + params->volFogColour2.B * k;

			float k2 = distanceShifted / colourThresh2 * k;
			if (k2 > 1) k2 = 1.0;
			kn = 1.0f - k2;
			fogTempR = fogTempR * kn + params->volFogColour3.R * k2;
			fogTempG = fogTempG * kn + params->volFogColour3.G * k2;
			fogTempB = fogTempB * kn + params->volFogColour3.B * k2;

			float fogDensity = 0.3f * fogIntensity * densityTemp / (1.0f + fogIntensity * densityTemp);
			if (fogDensity > 1) fogDensity = 1.0;

			output.R = fogDensity * fogTempR / 65536.0f + (1.0f - fogDensity) * output.R;
			output.G = fogDensity * fogTempG / 65536.0f + (1.0f - fogDensity) * output.G;
			output.B = fogDensity * fogTempB / 65536.0f + (1.0f - fogDensity) * output.B;

			totalOpacity = fogDensity + (1.0f - fogDensity) * totalOpacity;
			output.A = fogDensity + (1.0f - fogDensity) * output.A;
		}

		// iter fog
		if (params->iterFogEnabled)
		{
			int L = input.stepBuff[index].iters;
			float opacity = IterOpacity(step, L, params->N, params->iterFogOpacityTrim,
				params->iterFogOpacityTrimHigh, params->iterFogOpacity);

			sRGBAfloat newColour(0.0, 0.0, 0.0, 0.0);
			if (opacity > 0)
			{
				// fog colour
				float iterFactor1 = (L - params->iterFogOpacityTrim)
														/ (params->iterFogColor1Maxiter - params->iterFogOpacityTrim);
				float k = iterFactor1;
				if (k > 1.0f) k = 1.0f;
				if (k < 0.0f) k = 0.0f;
				float kn = 1.0f - k;
				float fogColR = params->iterFogColour1.R * kn + params->iterFogColour2.R * k;
				float fogColG = params->iterFogColour1.G * kn + params->iterFogColour2.G * k;
				float fogColB = params->iterFogColour1.B * kn + params->iterFogColour2.B * k;

				float iterFactor2 = (L - params->iterFogColor1Maxiter)
														/ (params->iterFogColor2Maxiter - params->iterFogColor1Maxiter);
				float k2 = iterFactor2;
				if (k2 < 0.0f) k2 = 0.0;
				if (k2 > 1.0f) k2 = 1.0;
				kn = 1.0f - k2;
				fogColR = fogColR * kn + params->iterFogColour3.R * k2;
				fogColG = fogColG * kn + params->iterFogColour3.G * k2;
				fogColB = fogColB * kn + params->iterFogColour3.B * k2;
				//----

				for (int i = 0; i < 5; i++)
				{
					if (i == 0)
					{
						if (params->mainLightEnable && params->mainLightIntensity > 0.0f)
						{
							sRGBAfloat shadowOutputTemp(1.0, 1.0, 1.0, 1.0);
							if (params->iterFogShadows)
							{
								shadowOutputTemp = MainShadow(input2);
							}
							else
							{
								shadowOutputTemp.R *= params->iterFogBrightnessBoost;
								shadowOutputTemp.G *= params->iterFogBrightnessBoost;
								shadowOutputTemp.B *= params->iterFogBrightnessBoost;
							}
							newColour.R += shadowOutputTemp.R * params->mainLightColour.R / 65536.0f
														 * params->mainLightIntensity;
							newColour.G += shadowOutputTemp.G * params->mainLightColour.G / 65536.0f
														 * params->mainLightIntensity;
							newColour.B += shadowOutputTemp.B * params->mainLightColour.B / 65536.0f
														 * params->mainLightIntensity;
						}
					}

					if (i > 0)
					{
						const sLight *light = data->lights.GetLight(i - 1);
						if (light->enabled)
						{
							CVector3 lightVectorTemp = light->position - point;
							double distanceLight = lightVectorTemp.Length();
							double distanceLight2 = distanceLight * distanceLight;
							lightVectorTemp.Normalize();

							double lightShadow = 1.0;
							if (params->iterFogShadows)
							{
								lightShadow = AuxShadow(input2, distanceLight, lightVectorTemp, light->intensity);
							}
							double intensity = light->intensity * params->iterFogBrightnessBoost;
							newColour.R += lightShadow * light->colour.R / 65536.0 / distanceLight2 * intensity;
							newColour.G += lightShadow * light->colour.G / 65536.0 / distanceLight2 * intensity;
							newColour.B += lightShadow * light->colour.B / 65536.0 / distanceLight2 * intensity;
						}
					}
				}

				if (params->ambientOcclusionEnabled
						&& params->ambientOcclusionMode == params::AOModeMultipleRays)
				{
					sRGBAfloat AO = AmbientOcclusion(input2);
					newColour.R += AO.R * params->ambientOcclusion;
					newColour.G += AO.G * params->ambientOcclusion;
					newColour.B += AO.B * params->ambientOcclusion;
				}

				if (opacity > 1.0f) opacity = 1.0f;

				output.R = output.R * (1.0f - opacity) + newColour.R * opacity * fogColR / 65536.0f;
				output.G = output.G * (1.0f - opacity) + newColour.G * opacity * fogColG / 65536.0f;
				output.B = output.B * (1.0f - opacity) + newColour.B * opacity * fogColB / 65536.0f;
				totalOpacity = opacity + (1.0f - opacity) * totalOpacity;
				output.A = opacity + (1.0f - opacity) * output.A;
			}
		}

		if (totalOpacity > 1.0f) totalOpacity = 1.0f;
		if (output.A > 1.0f) output.A = 1.0f;
		(*opacityOut).R = totalOpacity;
		(*opacityOut).G = totalOpacity;
		(*opacityOut).B = totalOpacity;

	} // next stepCount

	return output;
}
